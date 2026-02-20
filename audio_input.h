#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include "config.h"

// ============================================================================
//  AudioInput
//  Zbira audio vzorce iz I2S WM8782S (line-in) ali I2S INMP441 (MEMS mikrofon).
//  Teče na jedru 0 v svojem FreeRTOS tasku.
//  Ko je FFT_SAMPLES vzorcev pripravljenih, postavi zastavico.
// ============================================================================

class AudioInput {
public:
  // Inicializacija — zaženi task na jedru 0
  bool begin(uint8_t source);  // 0=off, 1=I2S line-in (WM8782S), 2=I2S mic (INMP441)
  void stop();

  // Dostop do vzorcev — kliče FFT procesiranje
  bool     samplesReady() const { return _ready; }
  void     consumeSamples();  // Označi, da so vzorci obdelani
  float*   getSamples() { return _samples; }  // FFT_SAMPLES float-ov

  // Statistika
  float    getPeakLevel() const { return _peakLevel; }
  uint32_t getSampleRate() const { return _actualSampleRate; }
  bool     isRunning() const { return _running; }
  uint8_t  getSource() const { return _source; }

private:
  float    _samples[FFT_SAMPLES];
  volatile bool _ready = false;
  volatile bool _running = false;
  uint8_t  _source = 0;
  float    _peakLevel = 0;
  uint32_t _actualSampleRate = 0;
  TaskHandle_t _taskHandle = nullptr;

  static void audioTask(void* param);
  void taskLoop();
  bool setupI2S(uint8_t source);
};

#endif
