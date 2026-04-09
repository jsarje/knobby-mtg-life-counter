#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "scr_st77916.h"
#include <lvgl.h>
#include "hal/lv_hal.h"
#include "knob.h"
#include "knob_hw.h"

static const int BATTERY_ADC_PIN = 1;
static const float BATTERY_DIVIDER_RATIO = 2.0f;
static const float BATTERY_CALIBRATION_SCALE = 1.0f;
static const float BATTERY_CALIBRATION_OFFSET = 0.0f;
static float battery_voltage_filtered = 0.0f;
static bool battery_voltage_has_value = false;

extern "C" float knob_read_battery_voltage(void)
{
  uint32_t millivolts = 0;
  uint32_t sum = 0;
  uint32_t min_sample = UINT32_MAX;
  uint32_t max_sample = 0;
  const int sample_count = 16;
  float measured_voltage = 0.0f;

  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  for (int i = 0; i < sample_count; i++) {
    uint32_t sample = analogReadMilliVolts(BATTERY_ADC_PIN);
    sum += sample;
    if (sample < min_sample) min_sample = sample;
    if (sample > max_sample) max_sample = sample;
    delayMicroseconds(250);
  }

  sum -= min_sample;
  sum -= max_sample;
  millivolts = sum / (sample_count - 2);
  if (millivolts == 0) {
    return 0.0f;
  }

  measured_voltage = (((float)millivolts * BATTERY_DIVIDER_RATIO) / 1000.0f);
  measured_voltage = (measured_voltage * BATTERY_CALIBRATION_SCALE) + BATTERY_CALIBRATION_OFFSET;

  if (!battery_voltage_has_value) {
    battery_voltage_filtered = measured_voltage;
    battery_voltage_has_value = true;
  } else {
    battery_voltage_filtered = (battery_voltage_filtered * 0.7f) + (measured_voltage * 0.3f);
  }

  return battery_voltage_filtered;
}

void setup()
{
  // Use the configured active CPU frequency for easier tuning.
  setCpuFrequencyMhz(CPU_FREQ_ACTIVE);

  // Disable radios
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  delay(200);
  Serial.begin(115200);

  scr_lvgl_init();
  knob_gui();

  // Keep RTC8M clock alive during light sleep so LEDC PWM (backlight) continues
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);
  gpio_sleep_sel_dis((gpio_num_t)TFT_BLK);

  // Configure light sleep wakeup sources
  gpio_wakeup_enable((gpio_num_t)TOUCH_PIN_NUM_INT, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  // Timer wakeup duration is set dynamically in loop() from lv_timer_handler()'s
  // next-deadline value so the CPU only wakes when LVGL actually needs to run.
}

// Maximum time the CPU may sleep before LVGL must get a tick, even if no timer fires.
// 2 seconds is conservative; LVGL's auto-dim check runs at 1 s so this is a safety net.
#define LIGHT_SLEEP_MAX_MS 2000U

void loop()
{
  static bool lvgl_suspended_for_dim = false;

  knob_process_pending();

  if (knob_is_dimmed()) {
    uint32_t time_till_next = LIGHT_SLEEP_MAX_MS;

    // While dimmed, avoid running LVGL timers and redraw work continuously.
    // Wake sources are GPIO (touch/rotary) plus a bounded timer safety net.
    lvgl_suspended_for_dim = true;

    // Clamp the sleep duration: never sleep longer than LIGHT_SLEEP_MAX_MS, and always
    // sleep at least 1 ms so we don't busy-spin if lv_timer_handler returns 0.
    if (time_till_next == 0 || time_till_next > LIGHT_SLEEP_MAX_MS)
      time_till_next = LIGHT_SLEEP_MAX_MS;

    // Arm the hardware timer to the next LVGL deadline before entering light sleep.
    esp_sleep_enable_timer_wakeup((uint64_t)time_till_next * 1000ULL);

    // Set knob pin wakeup to opposite of current level so any rotation wakes us
    uint8_t level_a = gpio_get_level((gpio_num_t)ROTARY_ENC_PIN_A);
    uint8_t level_b = gpio_get_level((gpio_num_t)ROTARY_ENC_PIN_B);
    gpio_wakeup_enable((gpio_num_t)ROTARY_ENC_PIN_A, level_a ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    gpio_wakeup_enable((gpio_num_t)ROTARY_ENC_PIN_B, level_b ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    esp_light_sleep_start();
    // GPIO wake = user input (touch or rotary): undim and resume the encoder timer
    // so knob events are processed on the next loop iteration.
    // Timer wake = LVGL housekeeping only: stay dimmed, re-enter sleep next iteration.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
      knob_notify_gpio_wakeup();
      activity_kick();
    }
  } else {
    uint32_t time_till_next;

    if (lvgl_suspended_for_dim) {
      lvgl_suspended_for_dim = false;
      lv_refr_now(NULL);
    }

    time_till_next = lv_timer_handler();

    // Disable knob pin wakeup when active so they don't interfere
    gpio_wakeup_disable((gpio_num_t)ROTARY_ENC_PIN_A);
    gpio_wakeup_disable((gpio_num_t)ROTARY_ENC_PIN_B);
    vTaskDelay(pdMS_TO_TICKS(time_till_next));
  }
}
