#include "audio_input.h"
#include "driver/i2s.h"

// Dvostranski buffer — medtem ko FFT bere enega, se drugi polni
static float _bufA[FFT_SAMPLES];
static float _bufB[FFT_SAMPLES];

bool AudioInput::begin(uint8_t source) {
  if (source == 0) return false;
  _source = source;
  _ready = false;
  _running = true;
  memset(_samples, 0, sizeof(_samples));
  memset(_bufA, 0, sizeof(_bufA));
  memset(_bufB, 0, sizeof(_bufB));

  if (!setupI2S(_source)) {
    Serial.println("[AUD] I2S inicializacija NAPAKA!");
    _running = false;
    return false;
  }

  // Zaženi task na jedru 0 (jedro 1 je za WiFi/DMX/Web)
  xTaskCreatePinnedToCore(audioTask, "audio", HAS_PSRAM ? 8192 : 4096, this, 3, &_taskHandle, 0);
  Serial.printf("[AUD] Audio task zagnan na jedru 0 (vir: %s)\n",
                _source == 1 ? "I2S line-in (WM8782S)" : "I2S mikrofon (INMP441)");
  return true;
}

void AudioInput::stop() {
  _running = false;
  if (_taskHandle) {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  Serial.println("[AUD] Audio ustavljeno");
}

void AudioInput::consumeSamples() {
  _ready = false;
}

bool AudioInput::setupI2S(uint8_t source) {
  i2s_config_t cfg = {};

  if (source == 1) {
    // WM8782S — ESP32 je slave (WM8782S generira BCLK in LRCK iz lastnega MCLK)
    cfg.mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX);
    cfg.sample_rate = WM8782S_SAMPLE_RATE;  // 48000 Hz
  } else {
    // INMP441 — ESP32 je master
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate = FFT_SAMPLE_RATE;      // 22050 Hz
  }

  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;

  i2s_pin_config_t pins = {};
  if (source == 1) {
    pins.bck_io_num  = I2S_LINE_BCK_PIN;
    pins.ws_io_num   = I2S_LINE_WS_PIN;
    pins.data_in_num = I2S_LINE_DATA_PIN;
  } else {
    pins.bck_io_num  = I2S_MIC_BCK_PIN;
    pins.ws_io_num   = I2S_MIC_WS_PIN;
    pins.data_in_num = I2S_MIC_DATA_PIN;
  }
  pins.data_out_num = I2S_PIN_NO_CHANGE;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;

  Serial.printf("[AUD] I2S inicializiran (%s, %s, %d Hz)\n",
                source == 1 ? "WM8782S" : "INMP441",
                source == 1 ? "slave" : "master",
                source == 1 ? WM8782S_SAMPLE_RATE : FFT_SAMPLE_RATE);
  return true;
}

// ============================================================================
//  TASK (jedro 0)
// ============================================================================

void AudioInput::audioTask(void* param) {
  AudioInput* self = (AudioInput*)param;
  self->taskLoop();
}

void AudioInput::taskLoop() {
  float* writeBuf = _bufA;
  bool useA = true;
  int sampleIdx = 0;
  unsigned long lastBatch = micros();
  int decimCounter = 0;  // Za decimacijo WM8782S (48kHz → ~24kHz)

  while (_running) {
    // I2S bere v blokih (oba vira)
    int32_t i2sBuf[64];
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, i2sBuf, sizeof(i2sBuf), &bytesRead, pdMS_TO_TICKS(10));
    int samplesRead = bytesRead / sizeof(int32_t);

    for (int i = 0; i < samplesRead && sampleIdx < FFT_SAMPLES; i++) {
      // WM8782S pri 48kHz: decimacija — obdrži vsak N-ti vzorec
      if (_source == 1) {
        if (++decimCounter < WM8782S_DECIMATION) continue;
        decimCounter = 0;
      }

      // Oba vira: 24-bit podatki v zgornjih bitih 32-bit besede
      int32_t val = i2sBuf[i] >> 8;  // Premakni na 24-bit
      writeBuf[sampleIdx] = val / 8388608.0f;  // Normaliziraj 24-bit
      sampleIdx++;
    }

    // Ko je buffer poln → zamenjaj bufferja
    if (sampleIdx >= FFT_SAMPLES) {
      unsigned long now = micros();
      unsigned long elapsed = now - lastBatch;
      _actualSampleRate = elapsed > 0
        ? (uint32_t)(1000000UL * FFT_SAMPLES / elapsed)
        : FFT_SAMPLE_RATE;
      lastBatch = now;

      // Izračunaj peak
      float peak = 0;
      for (int i = 0; i < FFT_SAMPLES; i++) {
        float absVal = writeBuf[i] < 0 ? -writeBuf[i] : writeBuf[i];
        if (absVal > peak) peak = absVal;
      }
      _peakLevel = peak;

      // Kopiraj v _samples (ki ga bere FFT)
      if (!_ready) {
        memcpy(_samples, writeBuf, sizeof(_samples));
        _ready = true;
      }

      // Zamenjaj buffer
      useA = !useA;
      writeBuf = useA ? _bufA : _bufB;
      sampleIdx = 0;
    }

    // I2S branje že vključuje čakanje (pdMS_TO_TICKS)
  }

  vTaskDelete(NULL);
}
