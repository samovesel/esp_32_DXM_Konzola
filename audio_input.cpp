#include "audio_input.h"
#include "driver/i2s.h"
#include "driver/adc.h"

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

  if (_source == 2) {
    if (!setupI2S()) {
      Serial.println("[AUD] I2S inicializacija NAPAKA!");
      _running = false;
      return false;
    }
  } else {
    // ADC setup
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);  // GPIO36, 0-3.3V
    Serial.println("[AUD] ADC inicializiran (GPIO36)");
  }

  // Zaženi task na jedru 0 (jedro 1 je za WiFi/DMX/Web)
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, this, 3, &_taskHandle, 0);
  Serial.printf("[AUD] Audio task zagnan na jedru 0 (vir: %s)\n",
                _source == 1 ? "ADC line-in" : "I2S mikrofon");
  return true;
}

void AudioInput::stop() {
  _running = false;
  if (_taskHandle) {
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  if (_source == 2) {
    i2s_driver_uninstall(I2S_NUM_0);
  }
  Serial.println("[AUD] Audio ustavljeno");
}

void AudioInput::consumeSamples() {
  _ready = false;
}

bool AudioInput::setupI2S() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = FFT_SAMPLE_RATE;
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
  pins.bck_io_num = I2S_BCK_PIN;
  pins.ws_io_num = I2S_WS_PIN;
  pins.data_in_num = I2S_DATA_PIN;
  pins.data_out_num = I2S_PIN_NO_CHANGE;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;

  Serial.println("[AUD] I2S inicializiran (INMP441)");
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

  while (_running) {
    // Zberi vzorce v aktivni buffer
    if (_source == 1) {
      // ADC: ročno vzorčenje s čakanjem za dosego želenega sample rate-a
      unsigned long targetInterval = 1000000UL / FFT_SAMPLE_RATE;  // ~45µs za 22050Hz

      for (int i = 0; i < 64 && sampleIdx < FFT_SAMPLES; i++) {
        unsigned long t0 = micros();

        int raw = adc1_get_raw(ADC1_CHANNEL_0);  // 0-4095
        // Odstrani DC offset (2048 = sredina) in normaliziraj na -1.0 ... 1.0
        writeBuf[sampleIdx] = (raw - 2048) / 2048.0f;
        sampleIdx++;

        // Čakaj za pravilen timing
        while ((micros() - t0) < targetInterval) { /* busy wait */ }
      }

    } else if (_source == 2) {
      // I2S bere v blokih
      int32_t i2sBuf[64];
      size_t bytesRead = 0;
      i2s_read(I2S_NUM_0, i2sBuf, sizeof(i2sBuf), &bytesRead, pdMS_TO_TICKS(10));
      int samplesRead = bytesRead / sizeof(int32_t);

      for (int i = 0; i < samplesRead && sampleIdx < FFT_SAMPLES; i++) {
        // INMP441 vrne 24-bit v zgornjih bitih 32-bit besede
        int32_t val = i2sBuf[i] >> 8;  // Premakni na 24-bit
        writeBuf[sampleIdx] = val / 8388608.0f;  // Normaliziraj 24-bit
        sampleIdx++;
      }
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

    // Kratka pavza da ne zasedemo jedra 100%
    if (_source == 2) {
      // I2S branje že vključuje čakanje (pdMS_TO_TICKS)
    } else {
      // ADC busy-wait je že zgoraj, ampak daj malo zraka
      taskYIELD();
    }
  }

  vTaskDelete(NULL);
}
