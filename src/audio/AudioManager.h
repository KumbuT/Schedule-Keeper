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

private:
  AudioManager() {}

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