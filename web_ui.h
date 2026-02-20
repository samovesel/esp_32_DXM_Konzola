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

void webBegin(AsyncWebServer* server, AsyncWebSocket* ws,
              NodeConfig* cfg, FixtureEngine* fixtures, MixerEngine* mixer,
              SceneEngine* scenes, SoundEngine* sound, AudioInput* audio);
void webSetLfoEngine(LfoEngine* lfo);
void webLoop();   // Periodično pošilja status prek WebSocket

#endif
