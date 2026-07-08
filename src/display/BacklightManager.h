#pragma once
#include <Arduino.h>

class BacklightManager {
public:
  static BacklightManager& instance();

  void begin(int pin, uint8_t fullBrightness = 255,
             uint32_t dimAfterMs = 60000,
             uint8_t  dimBrightness = 30);

  void wake();          // Call on any touch event — resets idle timer, full brightness
  void tick();          // Call every loop() — handles fade timing

  void setBrightness(uint8_t val);   // 0=off, 255=full
  bool isDimmed() const { return _dimmed; }

private:
  BacklightManager() {}

  int      _pin            = -1;
  uint8_t  _fullBrightness = 255;
  uint8_t  _dimBrightness  = 30;
  uint32_t _dimAfterMs     = 60000;
  uint32_t _lastActivity   = 0;
  bool     _dimmed         = false;
  uint8_t  _current        = 255;

  static constexpr int  PWM_CHANNEL = 0;   // LEDC channel — don't clash with others
  static constexpr int  PWM_FREQ    = 5000; // Hz — above audible range
  static constexpr int  PWM_RES     = 8;    // bits — 0–255
};
