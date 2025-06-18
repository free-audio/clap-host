#include <cassert>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QThread>
#include <QtGlobal>
#include <QtLogging>

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
   std::clog << "     ####### STOPPING ENGINE #########" << std::endl;
   stop();
   unloadPlugin();
   std::clog << "     ####### ENGINE STOPPED #########" << std::endl;
}

void Engine::allocateBuffers(size_t bufferSize) {
   freeBuffers();

   _inputs[0] = (float *)std::calloc(1, bufferSize);
   _inputs[1] = (float *)std::calloc(1, bufferSize);
   _outputs[0] = (float *)std::calloc(1, bufferSize);
   _outputs[1] = (float *)std::calloc(1, bufferSize);
}

void Engine::freeBuffers() {
   free(_inputs[0]);
   free(_inputs[1]);
   free(_outputs[0]);
   free(_outputs[1]);

   _inputs[0] = nullptr;
   _inputs[1] = nullptr;
   _outputs[0] = nullptr;
   _outputs[1] = nullptr;
}

void Engine::start() {
   assert(_state == kStateStopped);

   auto &as = _settings.audioSettings();

   /* midi */
   try {
      auto &deviceRef = _settings.midiSettings().deviceReference();
      _midiIn.reset();
      _midiIn = std::make_unique<RtMidiIn>(RtMidi::getCompiledApiByName(deviceRef._api.toStdString()));
      if (_midiIn) {
         _midiIn->openPort(deviceRef._index, "clap-host");
         _midiIn->ignoreTypes(false, false, false);
      }
   } catch (...) {
      _midiIn.reset();
   }

   /* audio */
   try {
      auto &deviceRef = as.deviceReference();
      unsigned int bufferSize = std::min<int>(32, as.bufferSize());

      // _audio->openStream() immediately calls process, so just allocate a big buffer for now...
      allocateBuffers(32 * 1024);

      _audio.reset();
      _audio =
         std::make_unique<RtAudio>(RtAudio::getCompiledApiByName(deviceRef._api.toStdString()));

      qInfo() << "Loading with Audio API:" << deviceRef._api;
      if (_audio) {
         const auto deviceIds = _audio->getDeviceIds();
         if (deviceIds.empty()) {
            qWarning() << "Can't activate audio engine: no audio devices";
            stop();
            return;
         }

         std::optional<int> deviceId;
         for (auto id : deviceIds) {
            const auto deviceInfo = _audio->getDeviceInfo(id);
            if (deviceRef._name.toStdString() != deviceInfo.name)
               continue;
            deviceId = id;
            break;
         }

         if (!deviceId.has_value()) {
            // At least we can try something...
            deviceId = _audio->getDefaultOutputDevice();
         }

         const auto deviceInfo = _audio->getDeviceInfo(deviceId.value());
         const auto& validSampleRates = deviceInfo.sampleRates;
         if (std::find(validSampleRates.begin(), validSampleRates.end(), as.sampleRate()) == validSampleRates.end()) {
            qWarning() << "The requested sample rate " << as.sampleRate() << " isn't supported by the selected output. Defaulting to " << deviceInfo.preferredSampleRate;
            as.setSampleRate(deviceInfo.preferredSampleRate);
         }

         RtAudio::StreamParameters outParams;
         outParams.deviceId = deviceId.value();
         outParams.firstChannel = 0;
         outParams.nChannels = 2;

         _audio->openStream(&outParams,
                            nullptr,
                            RTAUDIO_FLOAT32,
                            as.sampleRate(),
                            &bufferSize,
                            &Engine::audioCallback,
                            this);
         _nframes = bufferSize;

         _state = kStateRunning;

         _pluginHost->setPorts(2, _inputs, 2, _outputs);
         _pluginHost->activate(as.sampleRate(), _nframes);
         _audio->startStream();
      }
   } catch (...) {
      stop();
   }
}

void Engine::stop() {
   _pluginHost->deactivate();

   if (_state == kStateRunning)
      _state = kStateStopping;

   if (_audio) {
      if (_audio->isStreamOpen()) {
         _audio->stopStream();
         _audio->closeStream();
      }
      _audio.reset();
   }

   if (_midiIn) {
      if (_midiIn->isPortOpen())
         _midiIn->closePort();
      _midiIn.reset();
   }

   freeBuffers();

   _state = kStateStopped;
}

int Engine::audioCallback(void *outputBuffer,
                          void *inputBuffer,
                          const unsigned int frameCount,
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
      for (int i = 0; i < frameCount; ++i) {
         thiz->_inputs[0][i] = in[2 * i];
         thiz->_inputs[1][i] = in[2 * i + 1];
      }
   }

   thiz->_pluginHost->processBegin(frameCount);

   for (int i = 0; i < 8; i++) {
      uint32_t data;
      do data = thiz->keyboardNoteData[i];
#ifdef _WIN32
      while (data != InterlockedCompareExchange((LONG volatile *) &thiz->keyboardNoteData[i], 0, data));
#else
      while (data != __sync_val_compare_and_swap(&thiz->keyboardNoteData[i], data, 0));
#endif
      bool release = data & 0x8000;
      data &= ~0x8000;
      uint32_t note = 0;

      if (data == 'Z') note = 48; if (data == 'S') note = 49; if (data == 'X') note = 50; if (data == 'D') note = 51;
      if (data == 'C') note = 52; if (data == 'V') note = 53; if (data == 'G') note = 54; if (data == 'B') note = 55;
      if (data == 'H') note = 56; if (data == 'N') note = 57; if (data == 'J') note = 58; if (data == 'M') note = 59;
      if (data == ',') note = 60; if (data == 'l') note = 61; if (data == '.') note = 62; if (data == ';') note = 63;
      if (data == '/') note = 64; if (data == 'Q') note = 60; if (data == '2') note = 61; if (data == 'W') note = 62;
      if (data == '3') note = 63; if (data == 'E') note = 64; if (data == 'R') note = 65; if (data == '5') note = 66;
      if (data == 'T') note = 67; if (data == '6') note = 68; if (data == 'Y') note = 69; if (data == '7') note = 70;
      if (data == 'U') note = 71; if (data == 'I') note = 72; if (data == '9') note = 73; if (data == 'O') note = 74;
      if (data == '0') note = 75; if (data == 'P') note = 76; if (data == '[') note = 77; if (data == '=') note = 78;
      if (data == ']') note = 79;

      if (!note) {
      } else if (release) {
         thiz->_pluginHost->processNoteOff(0, 0, note, 100);
      } else {
         thiz->_pluginHost->processNoteOn(0, 0, note, 100);
      }
   }

   auto &midiBuf = thiz->_midiInBuffer;
   while (thiz->_midiIn && thiz->_midiIn->isPortOpen()) {
      auto msgTime = thiz->_midiIn->getMessage(&midiBuf);
      if (midiBuf.empty())
         break;

      uint8_t eventType = midiBuf[0] >> 4;
      uint8_t channel = midiBuf[0] & 0xf;
      uint8_t data1 = midiBuf[1];
      uint8_t data2 = midiBuf[2];

      double deltaMs = currentTime - msgTime;
      double deltaSample = (deltaMs * thiz->_sampleRate) / 1000;

      if (deltaSample >= frameCount)
         deltaSample = frameCount - 1;

      int32_t sampleOffset = frameCount - deltaSample;

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
   for (int i = 0; i < frameCount; ++i) {
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
