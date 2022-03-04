#pragma once

#include <array>
#include <memory>
#include <vector>

#include <QLibrary>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <RtAudio.h>
#include <RtMidi.h>

class Application;
class Settings;
class PluginHost;

class Engine : public QObject {
   Q_OBJECT

public:
   Engine(Application &application);
   ~Engine();

   enum State {
      kStateStopped,
      kStateRunning,
      kStateStopping,
   };

   void setParentWindow(WId parentWindow) { _parentWindow = parentWindow; }
   void start();
   void stop();

   bool loadPlugin(const QString &path, int plugin_index);
   void unloadPlugin();

   /* send events to the plugin from GUI */
   void setProgram(int8_t program, int8_t bank_msb, int8_t bank_lsb);
   void loadMidiFile(const QString &path);

   bool isRunning() const noexcept { return _state == kStateRunning; }
   int sampleRate() const noexcept { return _sampleRate; }

   PluginHost &pluginHost() const { return *_pluginHost; }

   auto midiIn() const { return _midiIn.get(); }
   auto audio() const { return _audio.get(); }

public:
   void callPluginIdle();

private:
   friend class AudioPlugin;
   friend class PluginHost;
   friend class Vst3Plugin;

   static int audioCallback(void *outputBuffer,
                            void *inputBuffer,
                            unsigned int nBufferFrames,
                            double streamTime,
                            RtAudioStreamStatus status,
                            void *data);

   void allocateBuffers(size_t bufferSize);
   void freeBuffers();

   Application &_application;
   Settings &_settings;
   WId _parentWindow;

   State _state = kStateStopped;

   /* audio & midi streams */
   std::unique_ptr<RtAudio> _audio;
   std::unique_ptr<RtMidiIn> _midiIn;

   /* engine context */
   int64_t _steadyTime = 0;
   int32_t _sampleRate = 44100;
   int32_t _nframes = 0;

   /* audio buffers */
   float *_inputs[2] = {nullptr, nullptr};
   float *_outputs[2] = {nullptr, nullptr};

   std::unique_ptr<PluginHost> _pluginHost;
   std::vector<unsigned char> _midiInBuffer;

   QTimer _idleTimer;
};
