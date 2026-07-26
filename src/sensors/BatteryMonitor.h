#pragma once
#include <Arduino.h>

class BatteryMonitor {
public:
  static BatteryMonitor& instance();
  void begin(int adcPin);
  void update();            // Call periodically (every 30s is fine)
  int  percentage() const { return _pct; }
  float voltage()   const { return _voltage; }

private:
  BatteryMonitor() {}
  int   _adcPin  = -1;
  int   _pct     = 0;
  float _voltage = 0.0f;

  // External voltage divider on the ADC pin: 200kΩ + 200kΩ, ratio = 0.5.
  // The XIAO ESP32-C3's battery pad is NOT wired to any ADC, so this external
  // divider is required to sense the cell (see README). Voltage is read via
  // analogReadMilliVolts() (factory-calibrated), so no fixed full-scale
  // constant is needed; attenuation is set to 11 dB (0–3.1 V) in begin().
  static constexpr float DIVIDER_RATIO = 0.5f;

  // LiPo voltage thresholds
  static constexpr float VBAT_FULL  = 4.20f;
  static constexpr float VBAT_EMPTY = 3.00f;

  float _readVoltage();
};
