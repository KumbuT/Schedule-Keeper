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
  // Average 16 samples to reduce ADC noise
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(_adcPin);
    delayMicroseconds(100);
  }
  float adcAvg  = sum / 16.0f;
  float adcMv   = (adcAvg / ADC_RESOLUTION) * ADC_MAX_MV;
  float batMv   = adcMv / DIVIDER_RATIO;
  return batMv / 1000.0f;  // Return in Volts
}

void BatteryMonitor::update() {
  _voltage = _readVoltage();
  float clamped = constrain(_voltage, VBAT_EMPTY, VBAT_FULL);
  _pct = (int)((clamped - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f);

  Serial.printf("[Battery] %.2fV → %d%%\n", _voltage, _pct);
}
