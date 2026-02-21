#ifndef WEB_UI_H
#define WEB_UI_H

#include <ESPAsyncWebServer.h>
#include "config.h"
#include "fixture_engine.h"
#include "mixer_engine.h"
#include "scene_engine.h"
#include "sound_engine.h"
#include "audio_input.h"
#include "lfo_engine.h"
#include "shape_engine.h"
#include "pixel_mapper.h"
#include "espnow_dmx.h"

void webBegin(AsyncWebServer* server, AsyncWebSocket* ws,
              NodeConfig* cfg, FixtureEngine* fixtures, MixerEngine* mixer,
              SceneEngine* scenes, SoundEngine* sound, AudioInput* audio);
void webSetLfoEngine(LfoEngine* lfo);
void webSetShapeGenerator(ShapeGenerator* shapes);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
void webSetPixelMapper(PixelMapper* px);
#endif
void webSetEspNow(EspNowDmx* espNow);
void webLoop();   // Periodično pošilja status prek WebSocket

#endif
