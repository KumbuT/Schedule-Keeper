#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <LittleFS.h>

class AudioManager {
public:
  static AudioManager& instance();
  void begin();
  void play(const char* path);  // path in LittleFS e.g. "/audio/starting.wav"
  void stop();
  void loop();                  // Call every loop() iteration
  bool isPlaying() const { return _playing; }

  // Touch feedback — generates a short tone directly into the I2S DMA buffer.
  // Non-blocking: fills one DMA buffer worth of samples and returns immediately.
  // freq: tone frequency in Hz (default 1200Hz — crisp without being harsh)
  // durationMs: tone length in milliseconds (default 18ms)
  void beep(uint16_t freq = 1200, uint16_t durationMs = 18);
  void setVolume(float vol) { _volume = vol; } // 0.0 to 1.0 (or higher)
  
private:
  AudioManager() {}
  float _volume = 1.0f; // Default to full volume

  static constexpr i2s_port_t I2S_PORT  = I2S_NUM_0;
  static constexpr int         PIN_BCLK  = 20;
  static constexpr int         PIN_LRCLK = 21;
  static constexpr int         PIN_DOUT  = 2;
  static constexpr size_t      CHUNK     = 512;  // bytes per DMA write

  File   _file;
  bool   _playing = false;

  void _installDriver();
  void _uninstallDriver();
  bool _consumeWavHeader();     // Reads 44-byte header, reconfigures I2S clock
};
