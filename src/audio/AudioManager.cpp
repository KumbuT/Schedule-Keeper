#include "AudioManager.h"

AudioManager& AudioManager::instance() {
  static AudioManager inst;
  return inst;
}

void AudioManager::_installDriver() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = 8000,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,   // more buffered audio (~128 ms @16 kHz) so a
    .dma_buf_len          = 256, // slow render frame can't drain it to underrun
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
  };

  i2s_pin_config_t pins = {
    .bck_io_num   = PIN_BCLK,
    .ws_io_num    = PIN_LRCLK,
    .data_out_num = PIN_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void AudioManager::_uninstallDriver() {
  i2s_driver_uninstall(I2S_PORT);
}

void AudioManager::begin() {
  _installDriver();
  Serial.println("[AudioManager] I2S ready");
}

// Reads the standard 44-byte WAV header, validates RIFF/WAVE markers,
// then reconfigures the I2S clock to match the file's actual sample rate
// and bit depth. This means you can mix 8kHz and 16kHz clips freely.
bool AudioManager::_consumeWavHeader() {
  // RIFF header: "RIFF" <riffSize> "WAVE"
  uint8_t riff[12];
  if (_file.read(riff, 12) != 12) return false;
  if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') return false;
  if (riff[8] != 'W' || riff[9] != 'A' || riff[10] != 'V' || riff[11] != 'E') return false;

  uint32_t sampleRate    = 16000;
  uint16_t bitsPerSample = 16;
  uint16_t channels      = 1;
  bool     haveFmt       = false;

  // Walk the chunk list to locate "fmt " and "data". Do NOT assume "data"
  // begins at byte 44: many encoders insert extra chunks (LIST/INFO, "fact",
  // etc.) before it. Assuming 44 plays those header bytes as PCM -- which is
  // exactly the "done.wav is just crackle while the others are fine" symptom
  // (that one file was authored with a non-standard header). Finding the real
  // data offset makes playback robust to any conformant WAV.
  uint8_t ch[8];
  while (_file.read(ch, 8) == 8) {
    uint32_t clen = (uint32_t)ch[4] | ((uint32_t)ch[5] << 8) |
                    ((uint32_t)ch[6] << 16) | ((uint32_t)ch[7] << 24);

    if (ch[0] == 'f' && ch[1] == 'm' && ch[2] == 't' && ch[3] == ' ') {
      uint8_t fmt[16];
      uint32_t want = clen < 16 ? clen : 16;
      if (_file.read(fmt, want) != (int)want) return false;
      channels      = (uint16_t)(fmt[2] | (fmt[3] << 8));
      sampleRate    = (uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) |
                      ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24);
      bitsPerSample = (uint16_t)(fmt[14] | (fmt[15] << 8));
      haveFmt = true;
      uint32_t rest = (clen > want ? clen - want : 0) + (clen & 1); // remainder + pad
      if (rest) _file.seek(_file.position() + rest);
    }
    else if (ch[0] == 'd' && ch[1] == 'a' && ch[2] == 't' && ch[3] == 'a') {
      if (!haveFmt) return false; // data before fmt -> malformed
      // _file is now positioned exactly at the PCM samples; loop() streams from here.
      Serial.printf("[AudioManager] WAV: %uHz %u-bit %uch (data @ byte %u)\n",
                    sampleRate, bitsPerSample, channels, (unsigned)_file.position());
      i2s_set_clk(I2S_PORT, sampleRate,
                  (i2s_bits_per_sample_t)bitsPerSample, I2S_CHANNEL_MONO);
      return true;
    }
    else {
      // Unknown chunk -- skip its (word-aligned) body and keep scanning.
      _file.seek(_file.position() + clen + (clen & 1));
    }
  }
  return false; // reached EOF without a data chunk
}

void AudioManager::play(const char* path) {
  stop();

  _file = LittleFS.open(path, "r");
  if (!_file) {
    Serial.printf("[AudioManager] Not found: %s\n", path);
    return;
  }

  if (!_consumeWavHeader()) {
    Serial.printf("[AudioManager] Bad WAV header: %s\n", path);
    _file.close();
    return;
  }

  _playing = true;
  Serial.printf("[AudioManager] Playing: %s\n", path);
}

void AudioManager::stop() {
  if (_file) _file.close();
  _playing = false;
  i2s_zero_dma_buffer(I2S_PORT);
}

// Called every loop() — tops up the I2S DMA buffer, feeding as many chunks as
// fit (non-blocking) rather than a single chunk per call. This decouples
// playback from the main-loop rate: previously one chunk was written per
// iteration, so when the loop slowed down (notably the task-complete
// celebration animation redrawing the whole screen at ~20 fps) the DMA
// underran and the audio broke up into gaps -- which truncated done.wav to
// only its clean tail ("...well done"). Now each call refills the buffer, so a
// slow render can't starve playback. Capped per call so a long clip can't hog
// the loop.
void AudioManager::loop() {
  if (!_playing || !_file) return;

  for (int i = 0; i < 16; i++) {
    uint8_t buf[CHUNK];
    size_t bytesRead = _file.read(buf, CHUNK);
    if (bytesRead == 0) {
      stop();  // EOF
      return;
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buf, bytesRead, &bytesWritten, 0);  // non-blocking

    if (bytesWritten < bytesRead) {
      // DMA is full -- rewind the unwritten remainder so we resume exactly
      // there on the next call (no dropped samples).
      _file.seek(_file.position() - (bytesRead - bytesWritten));
      return;
    }
  }
}

// ── Touch beep ────────────────────────────────────────────────────────────────
// Synthesises a pure square wave directly into a stack buffer and writes it
// to the I2S DMA in one call. No file I/O, no heap allocation.
// Does nothing if audio is muted (checked by the caller in main.cpp).
void AudioManager::beep(uint16_t freq, uint16_t durationMs) {
  // Reconfigure I2S clock for 16kHz — higher sample rate = cleaner beep
  i2s_set_clk(I2S_PORT, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  if(durationMs > 50) durationMs = 50;  // safety limit to avoid stack overflow
  const uint32_t sampleRate  = 16000;
  const uint32_t totalSamples= (sampleRate * durationMs) / 1000;
  const uint32_t halfPeriod  = sampleRate / (2 * freq);  // samples per half-cycle

  // Stack buffer — 16kHz * 18ms = 288 samples * 2 bytes = 576 bytes
  // Safe on the stack; keep durationMs ≤ 50 to avoid overflow
  const size_t bufSize = (totalSamples * sizeof(int16_t));
  int16_t buf[totalSamples];

  int16_t amplitude = 8000;  // ~25% of int16 max — loud enough, not distorted
  for (uint32_t i = 0; i < totalSamples; i++) {
    buf[i] = ((i / halfPeriod) % 2 == 0) ? amplitude : -amplitude;
  }

  size_t written = 0;
  i2s_write(I2S_PORT, buf, bufSize, &written, pdMS_TO_TICKS(50));

  // Restore to default 8kHz for WAV playback
  i2s_set_clk(I2S_PORT, 8000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_zero_dma_buffer(I2S_PORT);
}
