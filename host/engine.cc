#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QThread>
#include <QtGlobal>

#include "application.hh"
#include "engine.hh"
#include "main-window.hh"
#include "plugin-host.hh"
#include "settings.hh"

enum MidiStatus {
   MIDI_STATUS_NOTE_OFF = 0x8,
   MIDI_STATUS_NOTE_ON = 0x9,
   MIDI_STATUS_NOTE_AT = 0xA, // after touch
   MIDI_STATUS_CC = 0xB,      // control change
   MIDI_STATUS_PGM_CHANGE = 0xC,
   MIDI_STATUS_CHANNEL_AT = 0xD, // after touch
   MIDI_STATUS_PITCH_BEND = 0xE,
};

Engine::Engine(Application &application)
   : QObject(&application), _application(application), _settings(application.settings()),
     _idleTimer(this) {
   _pluginHost.reset(new PluginHost(*this));

   connect(&_idleTimer, &QTimer::timeout, this, QOverload<>::of(&Engine::callPluginIdle));
   _idleTimer.start(1000 / 30);

   _midiInBuffer.reserve(512);
}

Engine::~Engine() {
   std::clog << "     ####### STOPING ENGINE #########" << std::endl;
   stop();
   unloadPlugin();
   std::clog << "     ####### ENGINE STOPPED #########" << std::endl;
}

void Engine::start() {
   assert(_state == kStateStopped);

   auto &as = _settings.audioSettings();
   const int bufferSize = 4 * 2 * as.bufferSize();

   _inputs[0] = (float *)calloc(1, bufferSize);
   _inputs[1] = (float *)calloc(1, bufferSize);
   _outputs[0] = (float *)calloc(1, bufferSize);
   _outputs[1] = (float *)calloc(1, bufferSize);

   _pluginHost->setPorts(2, _inputs, 2, _outputs);

   /* midi */
   try {
      _midiIn.reset();
      _midiIn = std::make_unique<RtMidiIn>();
      if (_midiIn) {
         _midiIn->openPort(_settings.midiSettings().deviceReference()._index);
         _midiIn->ignoreTypes(false, false, false);
      }
   } catch (...) {
   }

   /* audio */
   try {
      auto &audioSettings = _settings.audioSettings();
      auto &deviceRef = audioSettings.deviceReference();
      unsigned int bufferSize = audioSettings.bufferSize();

      _audio.reset();
      _audio =
         std::make_unique<RtAudio>(RtAudio::getCompiledApiByName(deviceRef._api.toStdString()));
      if (_audio) {
         RtAudio::StreamParameters outParams;
         outParams.deviceId = deviceRef._index;
         outParams.firstChannel = 0;
         outParams.nChannels = 2;

         _audio->openStream(&outParams,
                            nullptr,
                            RTAUDIO_FLOAT32,
                            audioSettings.sampleRate(),
                            &bufferSize,
                            &Engine::audioCallback,
                            this);
         _nframes = bufferSize;
         _state = kStateRunning;
         _pluginHost->activate(as.sampleRate(), _nframes);
         _audio->startStream();
      }
   } catch (...) {
   }
}

void Engine::stop() {
   _pluginHost->deactivate();

   if (_state == kStateRunning)
      _state = kStateStopping;

   if (_audio->isStreamOpen()) {
      _audio->stopStream();
      _audio->closeStream();
      _audio.reset();
   }

   if (_midiIn->isPortOpen()) {
      _midiIn->closePort();
      _midiIn.reset();
   }

   _state = kStateStopped;
}

int Engine::audioCallback(void *outputBuffer,
                          void *inputBuffer,
                          unsigned int frameCount,
                          double currentTime,
                          RtAudioStreamStatus status,
                          void *data) {
   Engine *const thiz = (Engine *)data;
   const float *const in = (const float *)inputBuffer;
   float *const out = (float *)outputBuffer;

   assert(thiz->_inputs[0] != nullptr);
   assert(thiz->_inputs[1] != nullptr);
   assert(thiz->_outputs[0] != nullptr);
   assert(thiz->_outputs[1] != nullptr);
   assert(frameCount == thiz->_nframes);

   // copy input
   if (in) {
      for (int i = 0; i < thiz->_nframes; ++i) {
         thiz->_inputs[0][i] = in[2 * i];
         thiz->_inputs[1][i] = in[2 * i + 1];
      }
   }

   thiz->_pluginHost->processBegin(frameCount);

   MidiSettings &ms = thiz->_settings.midiSettings();
   auto &midiBuf = thiz->_midiInBuffer;
   while (thiz->_midiIn->isPortOpen()) {
      auto msgTime = thiz->_midiIn->getMessage(&midiBuf);
      if (midiBuf.empty())
         break;

      uint8_t eventType = midiBuf[0] >> 4;
      uint8_t channel = midiBuf[0] & 0xf;
      uint8_t data1 = midiBuf[1];
      uint8_t data2 = midiBuf[2];

      double deltaMs = currentTime - msgTime;
      double deltaSample = (deltaMs * thiz->_sampleRate) / 1000;

      if (deltaSample >= thiz->_nframes)
         deltaSample = thiz->_nframes - 1;

      int32_t sampleOffset = thiz->_nframes - deltaSample;

      switch (eventType) {
      case MIDI_STATUS_NOTE_ON:
         thiz->_pluginHost->processNoteOn(sampleOffset, channel, data1, data2);
         break;

      case MIDI_STATUS_NOTE_OFF:
         thiz->_pluginHost->processNoteOff(sampleOffset, channel, data1, data2);
         break;

      case MIDI_STATUS_CC:
         thiz->_pluginHost->processCC(sampleOffset, channel, data1, data2);
         break;

      case MIDI_STATUS_NOTE_AT:
         std::cerr << "Note AT key: " << (int)data1 << ", pres: " << (int)data2 << std::endl;
         thiz->_pluginHost->processNoteAt(sampleOffset, channel, data1, data2);
         break;

      case MIDI_STATUS_CHANNEL_AT:
         std::cerr << "Channel after touch" << std::endl;
         break;

      case MIDI_STATUS_PITCH_BEND:
         thiz->_pluginHost->processPitchBend(sampleOffset, channel, (data2 << 7) | data1);
         break;

      default:
         std::cerr << "unknown event type: " << (int)eventType << std::endl;
         break;
      }
   }

   thiz->_pluginHost->process();

   // copy output
   for (int i = 0; i < thiz->_nframes; ++i) {
      out[2 * i] = thiz->_outputs[0][i];
      out[2 * i + 1] = thiz->_outputs[1][i];
   }

   thiz->_steadyTime += frameCount;

   switch (thiz->_state) {
   case kStateRunning:
      return 0;
   case kStateStopping:
      thiz->_state = kStateStopped;
      return 1;
   default:
      assert(false && "unreachable");
      return 2;
   }
}

bool Engine::loadPlugin(const QString &path, int plugin_index) {
   if (!_pluginHost->load(path, plugin_index))
      return false;

   _pluginHost->setParentWindow(_parentWindow);
   return true;
}

void Engine::unloadPlugin() {
   _pluginHost->unload();

   free(_inputs[0]);
   free(_inputs[1]);
   free(_outputs[0]);
   free(_outputs[1]);

   _inputs[0] = nullptr;
   _inputs[1] = nullptr;
   _outputs[0] = nullptr;
   _outputs[1] = nullptr;
}

void Engine::callPluginIdle() {
   if (_pluginHost)
      _pluginHost->idle();
}
