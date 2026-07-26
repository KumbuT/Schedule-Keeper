#include "BatteryMonitor.h"
//static inline void delayMicroseconds(uint32_t us) { (void)us; }

BatteryMonitor& BatteryMonitor::instance() {
  static BatteryMonitor inst;
  return inst;
}

void BatteryMonitor::begin(int adcPin) {
  _adcPin = adcPin;
  analogSetAttenuation(ADC_11db);  // 0–3.1V range
  // Initial read
  update();
}

float BatteryMonitor::_readVoltage() {
  // Average 16 calibrated-millivolt samples. analogReadMilliVolts() applies the
  // ESP32-C3's per-chip factory ADC calibration (stored in eFuse), so it is far
  // more accurate than scaling a raw analogRead() by a fixed full-scale value --
  // that full scale varies ~10% chip-to-chip. Result lands within a few mV of a
  // multimeter.
  long sumMv = 0;
  for (int i = 0; i < 16; i++) {
    sumMv += analogReadMilliVolts(_adcPin);
    delayMicroseconds(100);
  }
  float adcMv = sumMv / 16.0f;         // calibrated pin voltage, mV
  float batMv = adcMv / DIVIDER_RATIO; // undo the 0.5 divider
  return batMv / 1000.0f;              // volts
}

void BatteryMonitor::update() {
  _voltage = _readVoltage();
  float clamped = constrain(_voltage, VBAT_EMPTY, VBAT_FULL);
  _pct = (int)((clamped - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f);

  Serial.printf("[Battery] %.2fV → %d%%\n", _voltage, _pct);
}
