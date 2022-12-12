#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QLibrary>
#include <QSemaphore>
#include <QSocketNotifier>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QWidget>

#include <clap/clap.h>
#include <clap/helpers/event-list.hh>
#include <clap/helpers/reducing-param-queue.hh>

#include "engine.hh"
#include "plugin-param.hh"

class Engine;
class PluginHostSettings;
class PluginHost final : public QObject {
   Q_OBJECT;

public:
   PluginHost(Engine &engine);
   ~PluginHost();

   bool load(const QString &path, int pluginIndex);
   void unload();

   bool canActivate() const;
   void activate(int32_t sample_rate, int32_t blockSize);
   void deactivate();

   void recreatePluginWindow();
   void setPluginWindowVisibility(bool isVisible);

   void setPorts(int numInputs, float **inputs, int numOutputs, float **outputs);
   void setParentWindow(WId parentWindow);

   void processBegin(int nframes);
   void processNoteOn(int sampleOffset, int channel, int key, int velocity);
   void processNoteOff(int sampleOffset, int channel, int key, int velocity);
   void processNoteAt(int sampleOffset, int channel, int key, int pressure);
   void processPitchBend(int sampleOffset, int channel, int value);
   void processCC(int sampleOffset, int channel, int cc, int value);
   void process();
   void processEnd(int nframes);

   void idle();

   void initPluginExtensions();
   void initThreadPool();
   void terminateThreadPool();
   void threadPoolEntry();

   void setParamValueByHost(PluginParam &param, double value);
   void setParamModulationByHost(PluginParam &param, double value);

   auto &params() const { return _params; }
   auto &quickControlsPages() const { return _quickControlsPages; }
   auto &quickControlsPagesIndex() const { return _quickControlsPagesIndex; }
   auto quickControlsSelectedPage() const { return _quickControlsSelectedPage; }
   void setQuickControlsSelectedPageByHost(clap_id page_id);

   bool loadNativePluginPreset(const std::string &path);
   bool loadStateFromFile(const std::string &path);
   bool saveStateToFile(const std::string &path);

   static void checkForMainThread();
   static void checkForAudioThread();

   QString paramValueToText(clap_id paramId, double value);

signals:
   void paramsChanged();
   void quickControlsPagesChanged();
   void quickControlsSelectedPageChanged();
   void paramAdjusted(clap_id paramId);

private:
   static PluginHost *fromHost(const clap_host *host);
   template <typename T>
   void initPluginExtension(const T *&ext, const char *id);

   /* clap host callbacks */
   static void clapLog(const clap_host *host, clap_log_severity severity, const char *msg);

   static void clapRequestCallback(const clap_host *host);
   static void clapRequestRestart(const clap_host *host);
   static void clapRequestProcess(const clap_host *host);

   static bool clapIsMainThread(const clap_host *host);
   static bool clapIsAudioThread(const clap_host *host);

   static void clapParamsRescan(const clap_host *host, clap_param_rescan_flags flags);
   static void
   clapParamsClear(const clap_host *host, clap_id param_id, clap_param_clear_flags flags);
   static void clapParamsRequestFlush(const clap_host *host);
   void scanParams();
   void scanParam(int32_t index);
   PluginParam &checkValidParamId(const std::string_view &function,
                                  const std::string_view &param_name,
                                  clap_id param_id);
   void checkValidParamValue(const PluginParam &param, double value);
   double getParamValue(const clap_param_info &info);
   static bool clapParamsRescanMayValueChange(uint32_t flags) {
      return flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_VALUES);
   }
   static bool clapParamsRescanMayInfoChange(uint32_t flags) {
      return flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_INFO);
   }

   void scanQuickControls();
   void quickControlsSetSelectedPage(clap_id pageId);
   static void clapQuickControlsChanged(const clap_host *host);

   static bool clapRegisterTimer(const clap_host *host, uint32_t period_ms, clap_id *timer_id);
   static bool clapUnregisterTimer(const clap_host *host, clap_id timer_id);
   static bool clapRegisterPosixFd(const clap_host *host, int fd, clap_posix_fd_flags_t flags);
   static bool clapModifyPosixFd(const clap_host *host, int fd, clap_posix_fd_flags_t flags);
   static bool clapUnregisterPosixFd(const clap_host *host, int fd);
   void eventLoopSetFdNotifierFlags(int fd, int flags);

   static bool clapThreadPoolRequestExec(const clap_host *host, uint32_t num_tasks);

   static const void *clapExtension(const clap_host *host, const char *extension);

   /* clap host gui callbacks */
   static void clapGuiResizeHintsChanged(const clap_host_t *host);
   static bool clapGuiRequestResize(const clap_host *host, uint32_t width, uint32_t height);
   static bool clapGuiRequestShow(const clap_host *host);
   static bool clapGuiRequestHide(const clap_host *host);
   static void clapGuiClosed(const clap_host *host, bool wasDestroyed);

   static void clapStateMarkDirty(const clap_host *host);

   bool canUsePluginParams() const noexcept;
   bool canUsePluginGui() const noexcept;
   static const char *getCurrentClapGuiApi();

   void paramFlushOnMainThread();
   void handlePluginOutputEvents();
   void generatePluginInputEvents();

private:
   Engine &_engine;
   PluginHostSettings &_settings;

   QLibrary _library;

   clap_host host_;
   static const constexpr clap_host_log _hostLog = {
      PluginHost::clapLog,
   };
   static const constexpr clap_host_gui _hostGui = {
      PluginHost::clapGuiResizeHintsChanged,
      PluginHost::clapGuiRequestResize,
      PluginHost::clapGuiRequestShow,
      PluginHost::clapGuiRequestHide,
      PluginHost::clapGuiClosed,
   };

   // static const constexpr clap_host_audio_ports hostAudioPorts_;
   // static const constexpr clap_host_audio_ports_config hostAudioPortsConfig_;
   static const constexpr clap_host_params _hostParams = {
      PluginHost::clapParamsRescan,
      PluginHost::clapParamsClear,
      PluginHost::clapParamsRequestFlush,
   };
   static const constexpr clap_host_quick_controls _hostQuickControls = {
      PluginHost::clapQuickControlsChanged,
   };
   static const constexpr clap_host_timer_support _hostTimerSupport = {
      PluginHost::clapRegisterTimer,
      PluginHost::clapUnregisterTimer,
   };
   static const constexpr clap_host_posix_fd_support _hostPosixFdSupport = {
      PluginHost::clapRegisterPosixFd,
      PluginHost::clapModifyPosixFd,
      PluginHost::clapUnregisterPosixFd,
   };

   static const constexpr clap_host_thread_check _hostThreadCheck = {
      PluginHost::clapIsMainThread,
      PluginHost::clapIsAudioThread,
   };
   static const constexpr clap_host_thread_pool _hostThreadPool = {
      PluginHost::clapThreadPoolRequestExec,
   };
   static const constexpr clap_host_state _hostState = {
      PluginHost::clapStateMarkDirty,
   };

   const clap_plugin_entry *_pluginEntry = nullptr;
   const clap_plugin_factory *_pluginFactory = nullptr;
   const clap_plugin *_plugin = nullptr;
   const clap_plugin_params *_pluginParams = nullptr;
   const clap_plugin_quick_controls *_pluginQuickControls = nullptr;
   const clap_plugin_audio_ports *_pluginAudioPorts = nullptr;
   const clap_plugin_gui *_pluginGui = nullptr;
   const clap_plugin_timer_support *_pluginTimerSupport = nullptr;
   const clap_plugin_posix_fd_support *_pluginPosixFdSupport = nullptr;
   const clap_plugin_thread_pool *_pluginThreadPool = nullptr;
   const clap_plugin_preset_load *_pluginPresetLoad = nullptr;
   const clap_plugin_state *_pluginState = nullptr;

   bool _pluginExtensionsAreInitialized = false;

   /* timers */
   clap_id _nextTimerId = 0;
   std::unordered_map<clap_id, std::unique_ptr<QTimer>> _timers;

   /* fd events */
   struct Notifiers {
      std::unique_ptr<QSocketNotifier> rd;
      std::unique_ptr<QSocketNotifier> wr;
   };
   std::unordered_map<int, std::unique_ptr<Notifiers>> _fds;

   /* thread pool */
   std::vector<std::unique_ptr<QThread>> _threadPool;
   std::atomic<bool> _threadPoolStop = {false};
   std::atomic<int> _threadPoolTaskIndex = {0};
   QSemaphore _threadPoolSemaphoreProd;
   QSemaphore _threadPoolSemaphoreDone;

   /* process stuff */
   clap_audio_buffer _audioIn = {};
   clap_audio_buffer _audioOut = {};
   clap::helpers::EventList _evIn;
   clap::helpers::EventList _evOut;
   clap_process _process;

   /* param update queues */
   std::unordered_map<clap_id, std::unique_ptr<PluginParam>> _params;

   struct AppToEngineParamQueueValue {
      void *cookie;
      double value;
   };

   struct EngineToAppParamQueueValue {
      void update(const EngineToAppParamQueueValue& v) noexcept {
         if (v.has_value) {
            has_value = true;
            value = v.value;
         }

         if (v.has_gesture) {
            has_gesture = true;
            is_begin = v.is_begin;
         }
      }

      bool has_value = false;
      bool has_gesture = false;
      bool is_begin = false;
      double value = 0;
   };

   clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue> _appToEngineValueQueue;
   clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue> _appToEngineModQueue;
   clap::helpers::ReducingParamQueue<clap_id, EngineToAppParamQueueValue> _engineToAppValueQueue;

   std::unordered_map<clap_id, bool> _isAdjustingParameter;

   std::vector<std::unique_ptr<clap_quick_controls_page>> _quickControlsPages;
   std::unordered_map<clap_id, clap_quick_controls_page *> _quickControlsPagesIndex;
   clap_id _quickControlsSelectedPage = CLAP_INVALID_ID;

   /* delayed actions */
   enum PluginState {
      // The plugin is inactive, only the main thread uses it
      Inactive,

      // Activation failed
      InactiveWithError,

      // The plugin is active and sleeping, the audio engine can call set_processing()
      ActiveAndSleeping,

      // The plugin is processing
      ActiveAndProcessing,

      // The plugin did process but is in error
      ActiveWithError,

      // The plugin is not used anymore by the audio engine and can be deactivated on the main
      // thread
      ActiveAndReadyToDeactivate,
   };

   bool isPluginActive() const;
   bool isPluginProcessing() const;
   bool isPluginSleeping() const;
   void setPluginState(PluginState state);

   PluginState _state = Inactive;
   bool _stateIsDirty = false;

   bool _scheduleRestart = false;
   bool _scheduleDeactivate = false;

   bool _scheduleProcess = true;

   bool _scheduleParamFlush = false;

   const char *_guiApi = nullptr;
   bool _isGuiCreated = false;
   bool _isGuiVisible = false;
   bool _isGuiFloating = false;

   bool _scheduleMainThreadCallback = false;
};
