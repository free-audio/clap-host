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
#include <clap/helpers/host.hh>
#include <clap/helpers/plugin-proxy.hh>

#include "engine.hh"
#include "plugin-param.hh"

class Engine;
class PluginHostSettings;

constexpr auto PluginHost_MH = clap::helpers::MisbehaviourHandler::Terminate;
constexpr auto PluginHost_CL = clap::helpers::CheckingLevel::Maximal;

using BaseHost = clap::helpers::Host<PluginHost_MH, PluginHost_CL>;
extern template class clap::helpers::Host<PluginHost_MH, PluginHost_CL>;

using PluginProxy = clap::helpers::PluginProxy<PluginHost_MH, PluginHost_CL>;
extern template class clap::helpers::PluginProxy<PluginHost_MH, PluginHost_CL>;

class PluginHost final : public QObject, public BaseHost {
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

   void initThreadPool();
   void terminateThreadPool();
   void threadPoolEntry();

   void setParamValueByHost(PluginParam &param, double value);
   void setParamModulationByHost(PluginParam &param, double value);

   auto &params() const { return _params; }
   auto &remoteControlsPages() const { return _remoteControlsPages; }
   auto &remoteControlsPagesIndex() const { return _remoteControlsPagesIndex; }
   auto remoteControlsSelectedPage() const { return _remoteControlsSelectedPage; }
   void setRemoteControlsSelectedPageByHost(clap_id page_id);

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
   void pluginLoadedChanged(bool pluginLoaded);

protected:
   /////////////////////////
   // clap::helpers::Host //
   /////////////////////////

   // clap_host
   void requestRestart() noexcept override;
   void requestProcess() noexcept override;
   void requestCallback() noexcept override;

   // clap_host_gui
   bool implementsGui() const noexcept override { return true; }
   void guiResizeHintsChanged() noexcept override;
   bool guiRequestResize(uint32_t width, uint32_t height) noexcept override;
   bool guiRequestShow() noexcept override;
   bool guiRequestHide() noexcept override;
   void guiClosed(bool wasDestroyed) noexcept override;

   // clap_host_log
   bool implementsLog() const noexcept override { return true; }
   void logLog(clap_log_severity severity, const char *message) const noexcept override;

   // clap_host_params
   bool implementsParams() const noexcept override { return true; }
   void paramsRescan(clap_param_rescan_flags flags) noexcept override;
   void paramsClear(clap_id paramId, clap_param_clear_flags flags) noexcept override;
   void paramsRequestFlush() noexcept override;

   // clap_host_posix_fd_support
   bool implementsPosixFdSupport() const noexcept override { return true; }
   bool posixFdSupportRegisterFd(int fd, clap_posix_fd_flags_t flags) noexcept override;
   bool posixFdSupportModifyFd(int fd, clap_posix_fd_flags_t flags) noexcept override;
   bool posixFdSupportUnregisterFd(int fd) noexcept override;

   // clap_host_remote_controls
   bool implementsRemoteControls() const noexcept override { return true; }
   void remoteControlsChanged() noexcept override;
   void remoteControlsSuggestPage(clap_id pageId) noexcept override;

   // clap_host_state
   bool implementsState() const noexcept override { return true; }
   void stateMarkDirty() noexcept override;

   // clap_host_timer_support
   bool implementsTimerSupport() const noexcept override { return true; }
   bool timerSupportRegisterTimer(uint32_t periodMs, clap_id *timerId) noexcept override;
   bool timerSupportUnregisterTimer(clap_id timerId) noexcept override;

   // clap_host_thread_check
   bool threadCheckIsMainThread() const noexcept override;
   bool threadCheckIsAudioThread() const noexcept override;

   // clap_host_thread_pool
   bool implementsThreadPool() const noexcept override { return true; }
   bool threadPoolRequestExec(uint32_t numTasks) noexcept override;

private:
   /* clap host callbacks */
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

   void eventLoopSetFdNotifierFlags(int fd, int flags);

   static const char *getCurrentClapGuiApi();

   void paramFlushOnMainThread();
   void handlePluginOutputEvents();
   void generatePluginInputEvents();

private:
   Engine &_engine;
   PluginHostSettings &_settings;

   QLibrary _library;

   const clap_plugin_entry *_pluginEntry = nullptr;
   const clap_plugin_factory *_pluginFactory = nullptr;
   std::unique_ptr<PluginProxy> _plugin;

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

   std::vector<std::unique_ptr<clap_remote_controls_page>> _remoteControlsPages;
   std::unordered_map<clap_id, clap_remote_controls_page *> _remoteControlsPagesIndex;
   clap_id _remoteControlsSelectedPage = CLAP_INVALID_ID;

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
