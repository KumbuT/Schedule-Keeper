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
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
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
  uint8_t header[44];
  if (_file.read(header, 44) != 44) return false;

  // Validate RIFF....WAVE marker
  if (header[0] != 'R' || header[1] != 'I' ||
      header[2] != 'F' || header[3] != 'F') return false;
  if (header[8]  != 'W' || header[9]  != 'A' ||
      header[10] != 'V' || header[11] != 'E') return false;

  // Sample rate: bytes 24–27 little-endian
  uint32_t sampleRate = header[24] | (header[25] << 8) |
                        (header[26] << 16) | (header[27] << 24);

  // Bits per sample: bytes 34–35 little-endian
  uint16_t bitsPerSample = header[34] | (header[35] << 8);

  Serial.printf("[AudioManager] WAV: %uHz %u-bit\n", sampleRate, bitsPerSample);

  i2s_set_clk(I2S_PORT,
               sampleRate,
               (i2s_bits_per_sample_t)bitsPerSample,
               I2S_CHANNEL_MONO);
  return true;
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

// Called every loop() — streams the next CHUNK of PCM data into the I2S
// DMA buffer. Non-blocking: i2s_write returns immediately if buffer is full.
void AudioManager::loop() {
  if (!_playing || !_file) return;

  uint8_t buf[CHUNK];
  size_t  bytesRead = _file.read(buf, CHUNK);

  if (bytesRead == 0) {
    stop();  // EOF
    return;
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_PORT, buf, bytesRead, &bytesWritten, portMAX_DELAY);
}

// ── Touch beep ────────────────────────────────────────────────────────────────
// Synthesises a pure square wave directly into a stack buffer and writes it
// to the I2S DMA in one call. No file I/O, no heap allocation.
// Does nothing if audio is muted (checked by the caller in main.cpp).
void AudioManager::beep(uint16_t freq, uint16_t durationMs) {
  // Reconfigure I2S clock for 16kHz — higher sample rate = cleaner beep
  i2s_set_clk(I2S_PORT, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

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
