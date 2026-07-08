#include "BacklightManager.h"

BacklightManager& BacklightManager::instance() {
  static BacklightManager inst;
  return inst;
}

void BacklightManager::begin(int pin, uint8_t fullBrightness,
                              uint32_t dimAfterMs, uint8_t dimBrightness) {
  _pin            = pin;
  _fullBrightness = fullBrightness;
  _dimAfterMs     = dimAfterMs;
  _dimBrightness  = dimBrightness;
  _lastActivity   = millis();

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(_pin, PWM_CHANNEL);
  setBrightness(_fullBrightness);

  Serial.printf("[Backlight] Init on GPIO %d, dim after %lums\n", _pin, _dimAfterMs);
}

void BacklightManager::setBrightness(uint8_t val) {
  _current = val;
  ledcWrite(PWM_CHANNEL, val);
}

void BacklightManager::wake() {
  _lastActivity = millis();
  if (_dimmed) {
    _dimmed = false;
    setBrightness(_fullBrightness);
    Serial.println("[Backlight] Wake");
  }
}

void BacklightManager::tick() {
  if (_dimmed) return;  // Already dimmed — nothing to do until wake()

  uint32_t idle = millis() - _lastActivity;

  if (idle >= _dimAfterMs) {
    // Smooth fade — step down 5 units every tick until dim level reached
    if (_current > _dimBrightness) {
      uint8_t next = (_current > _dimBrightness + 5)
                     ? _current - 5
                     : _dimBrightness;
      setBrightness(next);
    } else {
      _dimmed = true;
      Serial.println("[Backlight] Dimmed");
    }
  }
}
