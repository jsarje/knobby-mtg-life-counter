#include <Arduino.h>
#include <WiFi.h>

#include "esp_bt.h"
#include <lvgl.h>

#include "knob.h"
#include "knobby_app.h"
#include "platform_services.h"

static bool firmware_ready = false;

void knobby_app_setup(void)
{
  // 160 MHz uses less battery, but 240 MHz reduces rendering glitches and hangs here.
  setCpuFrequencyMhz(240);

  // Disable wireless radios.
  WiFi.mode(WIFI_OFF);
  btStop();

  delay(200);
  Serial.begin(115200);

  firmware_ready = knobby_platform_services_init_ui();
  if (!firmware_ready) {
    Serial.println("Firmware initialization failed; UI startup aborted.");
    return;
  }

  knob_gui();
}

void knobby_app_loop(void)
{
  if (!firmware_ready) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    return;
  }

  knob_process_pending();
  lv_timer_handler();
  vTaskDelay(5);
}
