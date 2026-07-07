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

  // Voltage divider: 100kΩ + 100kΩ, ratio = 0.5
  // ESP32-C3 ADC reference = 2500mV (attenuation DB_11 for 0–3.1V range)
  static constexpr float DIVIDER_RATIO  = 0.5f;
  static constexpr float ADC_MAX_MV     = 2500.0f;
  static constexpr int   ADC_RESOLUTION = 4095;

  // LiPo voltage thresholds
  static constexpr float VBAT_FULL  = 4.20f;
  static constexpr float VBAT_EMPTY = 3.00f;

  float _readVoltage();
};
