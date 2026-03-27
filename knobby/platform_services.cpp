#include "platform_services.h"

#include <Arduino.h>
#include <stdint.h>

#include "knob.h"
#include "scr_st77916.h"

namespace {

class BatteryService {
 public:
  float readVoltage() {
    uint32_t millivolts = 0;
    uint32_t sum = 0;
    uint32_t min_sample = UINT32_MAX;
    uint32_t max_sample = 0;
    const int sample_count = 16;
    float measured_voltage = 0.0f;

    analogSetPinAttenuation(kBatteryAdcPin, ADC_11db);
    for (int i = 0; i < sample_count; i++) {
      const uint32_t sample = analogReadMilliVolts(kBatteryAdcPin);
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

    measured_voltage = (((float)millivolts * kBatteryDividerRatio) / 1000.0f);
    measured_voltage = (measured_voltage * kBatteryCalibrationScale) + kBatteryCalibrationOffset;

    if (!has_filtered_voltage_) {
      filtered_voltage_ = measured_voltage;
      has_filtered_voltage_ = true;
    } else {
      filtered_voltage_ = (filtered_voltage_ * 0.7f) + (measured_voltage * 0.3f);
    }

    return filtered_voltage_;
  }

 private:
  static constexpr int kBatteryAdcPin = 1;
  static constexpr float kBatteryDividerRatio = 2.0f;
  static constexpr float kBatteryCalibrationScale = 1.0f;
  static constexpr float kBatteryCalibrationOffset = 0.0f;

  float filtered_voltage_ = 0.0f;
  bool has_filtered_voltage_ = false;
};

class BacklightService {
 public:
  void applyPercent(int percent) const {
    const uint8_t clamped_percent = clampPercent(percent);
    set_brightness(clamped_percent);
    screen_switch(clamped_percent > 0U);
  }

 private:
  static uint8_t clampPercent(int percent) {
    if (percent < 0) {
      return 0;
    }
    if (percent > 100) {
      return 100;
    }
    return static_cast<uint8_t>(percent);
  }
};

class UiPlatformService {
 public:
  bool initUi() const {
    return scr_lvgl_init();
  }
};

BatteryService g_battery_service;
BacklightService g_backlight_service;
UiPlatformService g_ui_platform_service;

}  // namespace

extern "C" bool knobby_platform_services_init_ui(void) {
  return g_ui_platform_service.initUi();
}

extern "C" void knobby_platform_apply_brightness_percent(int percent) {
  g_backlight_service.applyPercent(percent);
}

extern "C" float knob_read_battery_voltage(void) {
  return g_battery_service.readVoltage();
}
