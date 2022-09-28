#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

#include <QDebug>

#include "application.hh"
#include "engine.hh"
#include "main-window.hh"
#include "plugin-host-settings.hh"
#include "plugin-host.hh"
#include "settings.hh"

#include <clap/helpers/reducing-param-queue.hxx>

enum class ThreadType {
   Unknown,
   MainThread,
   AudioThread,
   AudioThreadPool,
};

thread_local ThreadType g_thread_type = ThreadType::Unknown;

PluginHost::PluginHost(Engine &engine)
   : QObject(&engine), _engine(engine), _settings(engine._settings.pluginHostSettings()) {
   g_thread_type = ThreadType::MainThread;

   host_.host_data = this;
   host_.clap_version = CLAP_VERSION;
   host_.name = "Clap Test Host";
   host_.version = "0.1.0";
   host_.vendor = "clap";
   host_.url = "https://github.com/free-audio/clap";
   host_.get_extension = PluginHost::clapExtension;
   host_.request_callback = PluginHost::clapRequestCallback;
   host_.request_process = PluginHost::clapRequestProcess;
   host_.request_restart = PluginHost::clapRequestRestart;

   initThreadPool();
}

PluginHost::~PluginHost() {
   checkForMainThread();

   terminateThreadPool();
}

void PluginHost::initThreadPool() {
   checkForMainThread();

   _threadPoolStop = false;
   _threadPoolTaskIndex = 0;
   auto N = QThread::idealThreadCount();
   _threadPool.resize(N);
   for (int i = 0; i < N; ++i) {
      _threadPool[i].reset(QThread::create(&PluginHost::threadPoolEntry, this));
      _threadPool[i]->start(QThread::HighestPriority);
   }
}

void PluginHost::terminateThreadPool() {
   checkForMainThread();

   _threadPoolStop = true;
   _threadPoolSemaphoreProd.release(_threadPool.size());
   for (auto &thr : _threadPool)
      if (thr)
         thr->wait();
}

void PluginHost::threadPoolEntry() {
   g_thread_type = ThreadType::AudioThreadPool;
   while (true) {
      _threadPoolSemaphoreProd.acquire();
      if (_threadPoolStop)
         return;

      int taskIndex = _threadPoolTaskIndex++;
      _pluginThreadPool->exec(_plugin, taskIndex);
      _threadPoolSemaphoreDone.release();
   }
}

bool PluginHost::load(const QString &path, int pluginIndex) {
   checkForMainThread();

   if (_library.isLoaded())
      unload();

   _library.setFileName(path);
   _library.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::DeepBindHint);
   if (!_library.load()) {
      QString err = _library.errorString();
      qWarning() << "Failed to load plugin '" << path << "': " << err;
      return false;
   }

   _pluginEntry =
      reinterpret_cast<const struct clap_plugin_entry *>(_library.resolve("clap_entry"));
   if (!_pluginEntry) {
      qWarning() << "Unable to resolve entry point 'clap_entry' in '" << path << "'";
      _library.unload();
      return false;
   }

   _pluginEntry->init(path.toStdString().c_str());

   _pluginFactory =
      static_cast<const clap_plugin_factory *>(_pluginEntry->get_factory(CLAP_PLUGIN_FACTORY_ID));

   auto count = _pluginFactory->get_plugin_count(_pluginFactory);
   if (pluginIndex > count) {
      qWarning() << "plugin index greater than count :" << count;
      return false;
   }

   auto desc = _pluginFactory->get_plugin_descriptor(_pluginFactory, pluginIndex);
   if (!desc) {
      qWarning() << "no plugin descriptor";
      return false;
   }

   if (!clap_version_is_compatible(desc->clap_version)) {
      qWarning() << "Incompatible clap version: Plugin is: " << desc->clap_version.major << "."
                 << desc->clap_version.minor << "." << desc->clap_version.revision << " Host is "
                 << CLAP_VERSION.major << "." << CLAP_VERSION.minor << "." << CLAP_VERSION.revision;
      return false;
   }

   _plugin = _pluginFactory->create_plugin(_pluginFactory, &host_, desc->id);
   if (!_plugin) {
      qWarning() << "could not create the plugin with id: " << desc->id;
      return false;
   }
   _plugin->init(_plugin);

   initPluginExtensions();
   scanParams();
   scanQuickControls();
   return true;
}

void PluginHost::initPluginExtensions() {
   checkForMainThread();

   if (_pluginExtensionsAreInitialized)
      return;

   initPluginExtension(_pluginParams, CLAP_EXT_PARAMS);
   initPluginExtension(_pluginQuickControls, CLAP_EXT_QUICK_CONTROLS);
   initPluginExtension(_pluginAudioPorts, CLAP_EXT_AUDIO_PORTS);
   initPluginExtension(_pluginGui, CLAP_EXT_GUI);
   initPluginExtension(_pluginTimerSupport, CLAP_EXT_TIMER_SUPPORT);
   initPluginExtension(_pluginPosixFdSupport, CLAP_EXT_POSIX_FD_SUPPORT);
   initPluginExtension(_pluginThreadPool, CLAP_EXT_THREAD_POOL);
   initPluginExtension(_pluginPresetLoad, CLAP_EXT_PRESET_LOAD);
   initPluginExtension(_pluginState, CLAP_EXT_STATE);

   _pluginExtensionsAreInitialized = true;
}

void PluginHost::unload() {
   checkForMainThread();

   if (!_library.isLoaded())
      return;

   if (_isGuiCreated) {
      _pluginGui->destroy(_plugin);
      _isGuiCreated = false;
      _isGuiVisible = false;
   }

   deactivate();

   if (_plugin) {
      _plugin->destroy(_plugin);
      _plugin = nullptr;
   }
   _pluginGui = nullptr;
   _pluginTimerSupport = nullptr;
   _pluginPosixFdSupport = nullptr;
   _pluginThreadPool = nullptr;
   _pluginPresetLoad = nullptr;
   _pluginState = nullptr;
   _pluginAudioPorts = nullptr;
   _pluginParams = nullptr;
   _pluginQuickControls = nullptr;

   _pluginEntry->deinit();
   _pluginEntry = nullptr;

   _library.unload();
}

bool PluginHost::canActivate() const {
   checkForMainThread();

   if (!_engine.isRunning())
      return false;
   if (isPluginActive())
      return false;
   if (_scheduleRestart)
      return false;
   return true;
}

void PluginHost::activate(int32_t sample_rate, int32_t blockSize) {
   checkForMainThread();

   if (!_plugin)
      return;

   assert(!isPluginActive());
   if (!_plugin->activate(_plugin, sample_rate, blockSize, blockSize)) {
      setPluginState(InactiveWithError);
      return;
   }

   _scheduleProcess = true;
   setPluginState(ActiveAndSleeping);
}

void PluginHost::deactivate() {
   checkForMainThread();

   if (!isPluginActive())
      return;

   while (isPluginProcessing() || isPluginSleeping()) {
      _scheduleDeactivate = true;
      QThread::msleep(10);
   }
   _scheduleDeactivate = false;

   _plugin->deactivate(_plugin);
   setPluginState(Inactive);
}

void PluginHost::setPorts(int numInputs, float **inputs, int numOutputs, float **outputs) {
   _audioIn.channel_count = numInputs;
   _audioIn.data32 = inputs;
   _audioIn.data64 = nullptr;
   _audioIn.constant_mask = 0;
   _audioIn.latency = 0;

   _audioOut.channel_count = numOutputs;
   _audioOut.data32 = outputs;
   _audioOut.data64 = nullptr;
   _audioOut.constant_mask = 0;
   _audioOut.latency = 0;
}

const char *PluginHost::getCurrentClapGuiApi() {
#if defined(Q_OS_LINUX)
   return CLAP_WINDOW_API_X11;
#elif defined(Q_OS_WIN)
   return CLAP_WINDOW_API_WIN32;
#elif defined(Q_OS_MACOS)
   return CLAP_WINDOW_API_COCOA;
#else
#   error "unsupported platform"
#endif
}

static clap_window makeClapWindow(WId window) {
   clap_window w;
#if defined(Q_OS_LINUX)
   w.api = CLAP_WINDOW_API_X11;
   w.x11 = window;
#elif defined(Q_OS_MACX)
   w.api = CLAP_WINDOW_API_COCOA;
   w.cocoa = reinterpret_cast<clap_nsview>(window);
#elif defined(Q_OS_WIN)
   w.api = CLAP_WINDOW_API_WIN32;
   w.win32 = reinterpret_cast<clap_hwnd>(window);
#endif

   return w;
}

void PluginHost::setParentWindow(WId parentWindow) {
   checkForMainThread();

   if (!canUsePluginGui())
      return;

   if (_isGuiCreated) {
      _pluginGui->destroy(_plugin);
      _isGuiCreated = false;
      _isGuiVisible = false;
   }

   _guiApi = getCurrentClapGuiApi();

   _isGuiFloating = false;
   if (!_pluginGui->is_api_supported(_plugin, _guiApi, false)) {
      if (!_pluginGui->is_api_supported(_plugin, _guiApi, true)) {
         qWarning() << "could find a suitable gui api";
         return;
      }
      _isGuiFloating = true;
   }

   auto w = makeClapWindow(parentWindow);
   if (!_pluginGui->create(_plugin, w.api, _isGuiFloating)) {
      qWarning() << "could not create the plugin gui";
      return;
   }

   _isGuiCreated = true;
   assert(_isGuiVisible == false);

   if (_isGuiFloating) {
      _pluginGui->set_transient(_plugin, &w);
      _pluginGui->suggest_title(_plugin, "using clap-host suggested title");
   } else {
      uint32_t width = 0;
      uint32_t height = 0;

      if (!_pluginGui->get_size(_plugin, &width, &height)) {
         qWarning() << "could not get the size of the plugin gui";
         _isGuiCreated = false;
         _pluginGui->destroy(_plugin);
         return;
      }

      Application::instance().mainWindow()->resizePluginView(width, height);

      if (!_pluginGui->set_parent(_plugin, &w)) {
         qWarning() << "could embbed the plugin gui";
         _isGuiCreated = false;
         _pluginGui->destroy(_plugin);
         return;
      }
   }

   setPluginWindowVisibility(true);
}

void PluginHost::setPluginWindowVisibility(bool isVisible) {
   checkForMainThread();

   if (!_isGuiCreated)
      return;

   if (isVisible && !_isGuiVisible) {
      _pluginGui->show(_plugin);
      _isGuiVisible = true;
   } else if (!isVisible && _isGuiVisible) {
      _pluginGui->hide(_plugin);
      _isGuiVisible = false;
   }
}

void PluginHost::clapRequestCallback(const clap_host *host) {
   auto h = fromHost(host);
   h->_scheduleMainThreadCallback = true;
}

void PluginHost::clapRequestProcess(const clap_host *host) {
   auto h = fromHost(host);
   h->_scheduleProcess = true;
}

void PluginHost::clapRequestRestart(const clap_host *host) {
   auto h = fromHost(host);
   h->_scheduleRestart = true;
}

void PluginHost::clapLog(const clap_host *host, clap_log_severity severity, const char *msg) {
   switch (severity) {
   case CLAP_LOG_DEBUG:
      qDebug() << msg;
      break;

   case CLAP_LOG_INFO:
      qInfo() << msg;
      break;

   case CLAP_LOG_WARNING:
   case CLAP_LOG_ERROR:
   case CLAP_LOG_FATAL:
   case CLAP_LOG_HOST_MISBEHAVING:
      qWarning() << msg;
      break;
   }
}

template <typename T>
void PluginHost::initPluginExtension(const T *&ext, const char *id) {
   checkForMainThread();

   if (!ext)
      ext = static_cast<const T *>(_plugin->get_extension(_plugin, id));
}

const void *PluginHost::clapExtension(const clap_host *host, const char *extension) {
   checkForMainThread();

   auto h = fromHost(host);

   if (!strcmp(extension, CLAP_EXT_GUI))
      return &h->_hostGui;
   if (!strcmp(extension, CLAP_EXT_LOG))
      return &h->_hostLog;
   if (!strcmp(extension, CLAP_EXT_THREAD_CHECK))
      return &h->_hostThreadCheck;
   if (!strcmp(extension, CLAP_EXT_THREAD_POOL))
      return &h->_hostThreadPool;
   if (!strcmp(extension, CLAP_EXT_TIMER_SUPPORT))
      return &h->_hostTimerSupport;
   if (!strcmp(extension, CLAP_EXT_POSIX_FD_SUPPORT))
      return &h->_hostPosixFdSupport;
   if (!strcmp(extension, CLAP_EXT_PARAMS))
      return &h->_hostParams;
   if (!strcmp(extension, CLAP_EXT_QUICK_CONTROLS))
      return &h->_hostQuickControls;
   if (!strcmp(extension, CLAP_EXT_STATE))
      return &h->_hostState;
   return nullptr;
}

PluginHost *PluginHost::fromHost(const clap_host *host) {
   if (!host)
      throw std::invalid_argument("Passed a null host pointer");

   auto h = static_cast<PluginHost *>(host->host_data);
   if (!h)
      throw std::invalid_argument("Passed an invalid host pointer because the host_data is null");

   if (!h->_plugin)
      throw std::logic_error("The plugin can't query for extensions during the create method. Wait "
                             "for clap_plugin.init() call.");

   return h;
}

bool PluginHost::clapIsMainThread(const clap_host *host) {
   return g_thread_type == ThreadType::MainThread;
}

bool PluginHost::clapIsAudioThread(const clap_host *host) {
   return g_thread_type == ThreadType::AudioThread;
}

void PluginHost::checkForMainThread() {
   if (g_thread_type != ThreadType::MainThread)
      throw std::logic_error("Requires Main Thread!");
}

void PluginHost::checkForAudioThread() {
   if (g_thread_type != ThreadType::AudioThread)
      throw std::logic_error("Requires Audio Thread!");
}

bool PluginHost::clapThreadPoolRequestExec(const clap_host *host, uint32_t num_tasks) {
   checkForAudioThread();

   auto h = fromHost(host);
   if (!h->_pluginThreadPool || !h->_pluginThreadPool->exec)
      throw std::logic_error("Called request_exec() without providing clap_plugin_thread_pool to "
                             "execute the job.");

   Q_ASSERT(!h->_threadPoolStop);
   Q_ASSERT(!h->_threadPool.empty());

   if (num_tasks == 0)
      return true;

   if (num_tasks == 1) {
      h->_pluginThreadPool->exec(h->_plugin, 0);
      return true;
   }

   h->_threadPoolTaskIndex = 0;
   h->_threadPoolSemaphoreProd.release(num_tasks);
   h->_threadPoolSemaphoreDone.acquire(num_tasks);
   return true;
}

bool PluginHost::clapRegisterTimer(const clap_host *host, uint32_t period_ms, clap_id *timer_id) {
   checkForMainThread();

   auto h = fromHost(host);
   h->initPluginExtensions();
   if (!h->_pluginTimerSupport || !h->_pluginTimerSupport->on_timer)
      throw std::logic_error(
         "Called register_timer() without providing clap_plugin_timer_support.on_timer() to "
         "receive the timer event.");

   auto id = h->_nextTimerId++;
   *timer_id = id;
   auto timer = std::make_unique<QTimer>();

   QObject::connect(timer.get(), &QTimer::timeout, [h, id] {
      checkForMainThread();
      h->_pluginTimerSupport->on_timer(h->_plugin, id);
   });

   auto t = timer.get();
   h->_timers.insert_or_assign(*timer_id, std::move(timer));
   t->start(period_ms);
   return true;
}

bool PluginHost::clapUnregisterTimer(const clap_host *host, clap_id timer_id) {
   checkForMainThread();

   auto h = fromHost(host);
   if (!h->_pluginTimerSupport || !h->_pluginTimerSupport->on_timer)
      throw std::logic_error(
         "Called unregister_timer() without providing clap_plugin_timer_support.on_timer() to "
         "receive the timer event.");

   auto it = h->_timers.find(timer_id);
   if (it == h->_timers.end())
      throw std::logic_error("Called unregister_timer() for a timer_id that was not registered.");

   h->_timers.erase(it);
   return true;
}

bool PluginHost::clapRegisterPosixFd(const clap_host *host, int fd, clap_posix_fd_flags_t flags) {
   checkForMainThread();

   auto h = fromHost(host);
   h->initPluginExtensions();
   if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
      throw std::logic_error("Called register_fd() without providing clap_plugin_fd_support to "
                             "receive the fd event.");

   auto it = h->_fds.find(fd);
   if (it != h->_fds.end())
      throw std::logic_error(
         "Called register_fd() for a fd that was already registered, use modify_fd() instead.");

   h->_fds.insert_or_assign(fd, std::make_unique<Notifiers>());
   h->eventLoopSetFdNotifierFlags(fd, flags);
   return true;
}

bool PluginHost::clapModifyPosixFd(const clap_host *host, int fd, clap_posix_fd_flags_t flags) {
   checkForMainThread();

   auto h = fromHost(host);
   if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
      throw std::logic_error("Called modify_fd() without providing clap_plugin_fd_support to "
                             "receive the fd event.");

   auto it = h->_fds.find(fd);
   if (it == h->_fds.end())
      throw std::logic_error(
         "Called modify_fd() for a fd that was not registered, use register_fd() instead.");

   h->_fds.insert_or_assign(fd, std::make_unique<Notifiers>());
   h->eventLoopSetFdNotifierFlags(fd, flags);
   return true;
}

bool PluginHost::clapUnregisterPosixFd(const clap_host *host, int fd) {
   checkForMainThread();

   auto h = fromHost(host);
   if (!h->_pluginPosixFdSupport || !h->_pluginPosixFdSupport->on_fd)
      throw std::logic_error("Called unregister_fd() without providing clap_plugin_fd_support to "
                             "receive the fd event.");

   auto it = h->_fds.find(fd);
   if (it == h->_fds.end())
      throw std::logic_error("Called unregister_fd() for a fd that was not registered.");

   h->_fds.erase(it);
   return true;
}

void PluginHost::eventLoopSetFdNotifierFlags(int fd, int flags) {
   checkForMainThread();

   auto it = _fds.find(fd);
   Q_ASSERT(it != _fds.end());

   if (flags & CLAP_POSIX_FD_READ) {
      if (!it->second->rd) {
         it->second->rd.reset(new QSocketNotifier((qintptr)fd, QSocketNotifier::Read));
         QObject::connect(it->second->rd.get(), &QSocketNotifier::activated, [this, fd] {
            checkForMainThread();
            _pluginPosixFdSupport->on_fd(this->_plugin, fd, CLAP_POSIX_FD_READ);
         });
      }
      it->second->rd->setEnabled(true);
   } else if (it->second->rd)
      it->second->rd->setEnabled(false);

   if (flags & CLAP_POSIX_FD_WRITE) {
      if (!it->second->wr) {
         it->second->wr.reset(new QSocketNotifier((qintptr)fd, QSocketNotifier::Write));
         QObject::connect(it->second->wr.get(), &QSocketNotifier::activated, [this, fd] {
            checkForMainThread();
            _pluginPosixFdSupport->on_fd(this->_plugin, fd, CLAP_POSIX_FD_WRITE);
         });
      }
      it->second->wr->setEnabled(true);
   } else if (it->second->wr)
      it->second->wr->setEnabled(false);
}

void PluginHost::clapGuiResizeHintsChanged(const clap_host_t *host) {
   // TODO
}

bool PluginHost::clapGuiRequestResize(const clap_host *host, uint32_t width, uint32_t height) {
   QMetaObject::invokeMethod(
      Application::instance().mainWindow(),
      [width, height] { Application::instance().mainWindow()->resizePluginView(width, height); },
      Qt::QueuedConnection);

   return true;
}

bool PluginHost::clapGuiRequestShow(const clap_host *host) {
   QMetaObject::invokeMethod(
      Application::instance().mainWindow(),
      [] { Application::instance().mainWindow()->showPluginWindow(); },
      Qt::QueuedConnection);

   return true;
}

bool PluginHost::clapGuiRequestHide(const clap_host *host) {
   QMetaObject::invokeMethod(
      Application::instance().mainWindow(),
      [] { Application::instance().mainWindow()->hidePluginWindow(); },
      Qt::QueuedConnection);

   return true;
}

void PluginHost::clapGuiClosed(const clap_host *host, bool wasDestroyed) { checkForMainThread(); }

void PluginHost::processBegin(int nframes) {
   g_thread_type = ThreadType::AudioThread;

   _process.frames_count = nframes;
   _process.steady_time = _engine._steadyTime;
}

void PluginHost::processEnd(int nframes) {
   g_thread_type = ThreadType::Unknown;

   _process.frames_count = nframes;
   _process.steady_time = _engine._steadyTime;
}

void PluginHost::processNoteOn(int sampleOffset, int channel, int key, int velocity) {
   checkForAudioThread();

   clap_event_note ev;
   ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   ev.header.type = CLAP_EVENT_NOTE_ON;
   ev.header.time = sampleOffset;
   ev.header.flags = 0;
   ev.header.size = sizeof(ev);
   ev.port_index = 0;
   ev.key = key;
   ev.channel = channel;
   ev.note_id = -1;
   ev.velocity = velocity / 127.0;

   _evIn.push(&ev.header);
}

void PluginHost::processNoteOff(int sampleOffset, int channel, int key, int velocity) {
   checkForAudioThread();

   clap_event_note ev;
   ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   ev.header.type = CLAP_EVENT_NOTE_OFF;
   ev.header.time = sampleOffset;
   ev.header.flags = 0;
   ev.header.size = sizeof(ev);
   ev.port_index = 0;
   ev.key = key;
   ev.channel = channel;
   ev.note_id = -1;
   ev.velocity = velocity / 127.0;

   _evIn.push(&ev.header);
}

void PluginHost::processNoteAt(int sampleOffset, int channel, int key, int pressure) {
   checkForAudioThread();

   // TODO
}

void PluginHost::processPitchBend(int sampleOffset, int channel, int value) {
   checkForAudioThread();

   // TODO
}

void PluginHost::processCC(int sampleOffset, int channel, int cc, int value) {
   checkForAudioThread();

   clap_event_midi ev;
   ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
   ev.header.type = CLAP_EVENT_MIDI;
   ev.header.time = sampleOffset;
   ev.header.flags = 0;
   ev.header.size = sizeof(ev);
   ev.port_index = 0;
   ev.data[0] = 0xB0 | channel;
   ev.data[1] = cc;
   ev.data[2] = value;

   _evIn.push(&ev.header);
}

void PluginHost::process() {
   checkForAudioThread();

   if (!_plugin)
      return;

   // Can't process a plugin that is not active
   if (!isPluginActive())
      return;

   // Do we want to deactivate the plugin?
   if (_scheduleDeactivate) {
      _scheduleDeactivate = false;
      if (_state == ActiveAndProcessing)
         _plugin->stop_processing(_plugin);
      setPluginState(ActiveAndReadyToDeactivate);
      return;
   }

   // We can't process a plugin which failed to start processing
   if (_state == ActiveWithError)
      return;

   _process.transport = nullptr;

   _process.in_events = _evIn.clapInputEvents();
   _process.out_events = _evOut.clapOutputEvents();

   _process.audio_inputs = &_audioIn;
   _process.audio_inputs_count = 1;
   _process.audio_outputs = &_audioOut;
   _process.audio_outputs_count = 1;

   _evOut.clear();
   generatePluginInputEvents();

   if (isPluginSleeping()) {
      if (!_scheduleProcess && _evIn.empty())
         // The plugin is sleeping, there is no request to wake it up and there are no events to
         // process
         return;

      _scheduleProcess = false;
      if (!_plugin->start_processing(_plugin)) {
         // the plugin failed to start processing
         setPluginState(ActiveWithError);
         return;
      }

      setPluginState(ActiveAndProcessing);
   }

   int32_t status = CLAP_PROCESS_SLEEP;
   if (isPluginProcessing())
      status = _plugin->process(_plugin, &_process);

   handlePluginOutputEvents();

   _evOut.clear();
   _evIn.clear();

   _engineToAppValueQueue.producerDone();

   // TODO: send plugin to sleep if possible

   g_thread_type = ThreadType::Unknown;
}

void PluginHost::generatePluginInputEvents() {
   _appToEngineValueQueue.consume(
      [this](clap_id param_id, const AppToEngineParamQueueValue &value) {
         clap_event_param_value ev;
         ev.header.time = 0;
         ev.header.type = CLAP_EVENT_PARAM_VALUE;
         ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
         ev.header.flags = 0;
         ev.header.size = sizeof(ev);
         ev.param_id = param_id;
         ev.cookie = _settings.shouldProvideCookie() ? value.cookie : nullptr;
         ev.port_index = 0;
         ev.key = -1;
         ev.channel = -1;
         ev.note_id = -1;
         ev.value = value.value;
         _evIn.push(&ev.header);
      });

   _appToEngineModQueue.consume([this](clap_id param_id, const AppToEngineParamQueueValue &value) {
      clap_event_param_mod ev;
      ev.header.time = 0;
      ev.header.type = CLAP_EVENT_PARAM_MOD;
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.flags = 0;
      ev.header.size = sizeof(ev);
      ev.param_id = param_id;
      ev.cookie = _settings.shouldProvideCookie() ? value.cookie : nullptr;
      ev.port_index = 0;
      ev.key = -1;
      ev.channel = -1;
      ev.note_id = -1;
      ev.amount = value.value;
      _evIn.push(&ev.header);
   });
}

void PluginHost::handlePluginOutputEvents() {
   for (uint32_t i = 0; i < _evOut.size(); ++i) {
      auto h = _evOut.get(i);
      switch (h->type) {
      case CLAP_EVENT_PARAM_GESTURE_BEGIN: {
         auto ev = reinterpret_cast<const clap_event_param_gesture *>(h);
         bool &isAdj = _isAdjustingParameter[ev->param_id];

         if (isAdj)
            throw std::logic_error("The plugin sent BEGIN_ADJUST twice");
         isAdj = true;

         EngineToAppParamQueueValue v;
         v.has_gesture = true;
         v.is_begin = true;
         _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
         break;
      }

      case CLAP_EVENT_PARAM_GESTURE_END: {
         auto ev = reinterpret_cast<const clap_event_param_gesture *>(h);
         bool &isAdj = _isAdjustingParameter[ev->param_id];

         if (!isAdj)
            throw std::logic_error("The plugin sent END_ADJUST without a preceding BEGIN_ADJUST");
         isAdj = false;
         EngineToAppParamQueueValue v;
         v.has_gesture = true;
         v.is_begin = false;
         _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
         break;
      }

      case CLAP_EVENT_PARAM_VALUE: {
         auto ev = reinterpret_cast<const clap_event_param_value *>(h);
         EngineToAppParamQueueValue v;
         v.has_value = true;
         v.value = ev->value;
         _engineToAppValueQueue.setOrUpdate(ev->param_id, v);
         break;
      }
      }
   }
}

void PluginHost::paramFlushOnMainThread() {
   checkForMainThread();

   assert(!isPluginActive());

   _scheduleParamFlush = false;

   _evIn.clear();
   _evOut.clear();

   generatePluginInputEvents();

   if (canUsePluginParams())
      _pluginParams->flush(_plugin, _evIn.clapInputEvents(), _evOut.clapOutputEvents());
   handlePluginOutputEvents();

   _evOut.clear();
   _engineToAppValueQueue.producerDone();
}

void PluginHost::idle() {
   checkForMainThread();

   // Try to send events to the audio engine
   _appToEngineValueQueue.producerDone();
   _appToEngineModQueue.producerDone();

   _engineToAppValueQueue.consume(
      [this](clap_id param_id, const EngineToAppParamQueueValue &value) {
         auto it = _params.find(param_id);
         if (it == _params.end()) {
            std::ostringstream msg;
            msg << "Plugin produced a CLAP_EVENT_PARAM_SET with an unknown param_id: " << param_id;
            throw std::invalid_argument(msg.str());
         }

         if (value.has_value)
            it->second->setValue(value.value);

         if (value.has_gesture)
            it->second->setIsAdjusting(value.is_begin);

         emit paramAdjusted(param_id);
      });

   if (_scheduleParamFlush && !isPluginActive()) {
      paramFlushOnMainThread();
   }

   if (_scheduleMainThreadCallback) {
      _scheduleMainThreadCallback = false;
      _plugin->on_main_thread(_plugin);
   }

   if (_scheduleRestart) {
      deactivate();
      _scheduleRestart = false;
      activate(_engine._sampleRate, _engine._nframes);
   }
}

PluginParam &PluginHost::checkValidParamId(const std::string_view &function,
                                           const std::string_view &param_name,
                                           clap_id param_id) {
   checkForMainThread();

   if (param_id == CLAP_INVALID_ID) {
      std::ostringstream msg;
      msg << "Plugin called " << function << " with " << param_name << " == CLAP_INVALID_ID";
      throw std::invalid_argument(msg.str());
   }

   auto it = _params.find(param_id);
   if (it == _params.end()) {
      std::ostringstream msg;
      msg << "Plugin called " << function << " with  an invalid " << param_name
          << " == " << param_id;
      throw std::invalid_argument(msg.str());
   }

   Q_ASSERT(it->first == param_id);
   Q_ASSERT(it->second->info().id == param_id);
   return *it->second;
}

void PluginHost::checkValidParamValue(const PluginParam &param, double value) {
   checkForMainThread();
   if (!param.isValueValid(value)) {
      std::ostringstream msg;
      msg << "Invalid value for param. ";
      param.printInfo(msg);
      msg << "; value: " << value;
      // std::cerr << msg.str() << std::endl;
      throw std::invalid_argument(msg.str());
   }
}

void PluginHost::setParamValueByHost(PluginParam &param, double value) {
   checkForMainThread();

   param.setValue(value);

   _appToEngineValueQueue.set(param.info().id, {param.info().cookie, value});
   _appToEngineValueQueue.producerDone();
   clapParamsRequestFlush(&host_);
}

void PluginHost::setParamModulationByHost(PluginParam &param, double value) {
   checkForMainThread();

   param.setModulation(value);

   _appToEngineModQueue.set(param.info().id, {param.info().cookie, value});
   _appToEngineModQueue.producerDone();
   clapParamsRequestFlush(&host_);
}

void PluginHost::scanParams() { clapParamsRescan(&host_, CLAP_PARAM_RESCAN_ALL); }

void PluginHost::clapParamsRescan(const clap_host *host, uint32_t flags) {
   checkForMainThread();
   auto h = fromHost(host);

   if (!h->canUsePluginParams())
      return;

   // 1. it is forbidden to use CLAP_PARAM_RESCAN_ALL if the plugin is active
   if (h->isPluginActive() && (flags & CLAP_PARAM_RESCAN_ALL)) {
      throw std::logic_error(
         "clap_host_params.recan(CLAP_PARAM_RESCAN_ALL) was called while the plugin is active!");
      return;
   }

   // 2. scan the params.
   auto count = h->_pluginParams->count(h->_plugin);
   std::unordered_set<clap_id> paramIds(count * 2);

   for (int32_t i = 0; i < count; ++i) {
      clap_param_info info;
      if (!h->_pluginParams->get_info(h->_plugin, i, &info))
         throw std::logic_error("clap_plugin_params.get_info did return false!");

      if (info.id == CLAP_INVALID_ID) {
         std::ostringstream msg;
         msg << "clap_plugin_params.get_info() reported a parameter with id = CLAP_INVALID_ID"
             << std::endl
             << " 2. name: " << info.name << ", module: " << info.module << std::endl;
         throw std::logic_error(msg.str());
      }

      auto it = h->_params.find(info.id);

      // check that the parameter is not declared twice
      if (paramIds.count(info.id) > 0) {
         Q_ASSERT(it != h->_params.end());

         std::ostringstream msg;
         msg << "the parameter with id: " << info.id << " was declared twice." << std::endl
             << " 1. name: " << it->second->info().name << ", module: " << it->second->info().module
             << std::endl
             << " 2. name: " << info.name << ", module: " << info.module << std::endl;
         throw std::logic_error(msg.str());
      }
      paramIds.insert(info.id);

      if (it == h->_params.end()) {
         if (!(flags & CLAP_PARAM_RESCAN_ALL)) {
            std::ostringstream msg;
            msg << "a new parameter was declared, but the flag CLAP_PARAM_RESCAN_ALL was not "
                   "specified; id: "
                << info.id << ", name: " << info.name << ", module: " << info.module << std::endl;
            throw std::logic_error(msg.str());
         }

         double value = h->getParamValue(info);
         auto param = std::make_unique<PluginParam>(*h, info, value);
         h->checkValidParamValue(*param, value);
         h->_params.insert_or_assign(info.id, std::move(param));
      } else {
         // update param info
         if (!it->second->isInfoEqualTo(info)) {
            if (!clapParamsRescanMayInfoChange(flags)) {
               std::ostringstream msg;
               msg << "a parameter's info did change, but the flag CLAP_PARAM_RESCAN_INFO "
                      "was not specified; id: "
                   << info.id << ", name: " << info.name << ", module: " << info.module
                   << std::endl;
               throw std::logic_error(msg.str());
            }

            if (!(flags & CLAP_PARAM_RESCAN_ALL) &&
                !it->second->isInfoCriticallyDifferentTo(info)) {
               std::ostringstream msg;
               msg << "a parameter's info has critical changes, but the flag CLAP_PARAM_RESCAN_ALL "
                      "was not specified; id: "
                   << info.id << ", name: " << info.name << ", module: " << info.module
                   << std::endl;
               throw std::logic_error(msg.str());
            }

            it->second->setInfo(info);
         }

         double value = h->getParamValue(info);
         if (it->second->value() != value) {
            if (!clapParamsRescanMayValueChange(flags)) {
               std::ostringstream msg;
               msg << "a parameter's value did change but, but the flag CLAP_PARAM_RESCAN_VALUES "
                      "was not specified; id: "
                   << info.id << ", name: " << info.name << ", module: " << info.module
                   << std::endl;
               throw std::logic_error(msg.str());
            }

            // update param value
            h->checkValidParamValue(*it->second, value);
            it->second->setValue(value);
            it->second->setModulation(value);
         }
      }
   }

   // remove parameters which are gone
   for (auto it = h->_params.begin(); it != h->_params.end();) {
      if (paramIds.find(it->first) != paramIds.end())
         ++it;
      else {
         if (!(flags & CLAP_PARAM_RESCAN_ALL)) {
            std::ostringstream msg;
            auto &info = it->second->info();
            msg << "a parameter was removed, but the flag CLAP_PARAM_RESCAN_ALL was not "
                   "specified; id: "
                << info.id << ", name: " << info.name << ", module: " << info.module << std::endl;
            throw std::logic_error(msg.str());
         }
         it = h->_params.erase(it);
      }
   }

   if (flags & CLAP_PARAM_RESCAN_ALL)
      h->paramsChanged();
}

void PluginHost::clapParamsClear(const clap_host *host,
                                 clap_id param_id,
                                 clap_param_clear_flags flags) {
   checkForMainThread();
}

void PluginHost::clapParamsRequestFlush(const clap_host *host) {
   auto self = fromHost(host);

   if (!self->isPluginActive() && clapIsMainThread(host)) {
      // Perform the flush immediately
      self->paramFlushOnMainThread();
      return;
   }

   self->_scheduleParamFlush = true;
   return;
}

double PluginHost::getParamValue(const clap_param_info &info) {
   checkForMainThread();

   if (!canUsePluginParams())
      return 0;

   double value;
   if (_pluginParams->get_value(_plugin, info.id, &value))
      return value;

   std::ostringstream msg;
   msg << "failed to get the param value, id: " << info.id << ", name: " << info.name
       << ", module: " << info.module;
   throw std::logic_error(msg.str());
}

void PluginHost::scanQuickControls() {
   checkForMainThread();

   if (!_pluginQuickControls)
      return;

   if (!_pluginQuickControls->get || !_pluginQuickControls->count) {
      std::ostringstream msg;
      msg << "clap_plugin_quick_controls is partially implemented.";
      throw std::logic_error(msg.str());
   }

   quickControlsSetSelectedPage(CLAP_INVALID_ID);
   _quickControlsPages.clear();
   _quickControlsPagesIndex.clear();

   const auto N = _pluginQuickControls->count(_plugin);
   if (N == 0)
      return;

   _quickControlsPages.reserve(N);
   _quickControlsPagesIndex.reserve(N);

   clap_id firstPageId = CLAP_INVALID_ID;
   for (int i = 0; i < N; ++i) {
      auto page = std::make_unique<clap_quick_controls_page>();
      if (!_pluginQuickControls->get(_plugin, i, page.get())) {
         std::ostringstream msg;
         msg << "clap_plugin_quick_controls.get_page(" << i << ") failed, while the page count is "
             << N;
         throw std::logic_error(msg.str());
      }

      if (page->id == CLAP_INVALID_ID) {
         std::ostringstream msg;
         msg << "clap_plugin_quick_controls.get_page(" << i << ") gave an invalid page_id";
         throw std::invalid_argument(msg.str());
      }

      if (i == 0)
         firstPageId = page->id;

      auto it = _quickControlsPagesIndex.find(page->id);
      if (it != _quickControlsPagesIndex.end()) {
         std::ostringstream msg;
         msg << "clap_plugin_quick_controls.get_page(" << i
             << ") gave twice the same page_id:" << page->id << std::endl
             << " 1. name: " << it->second->name << std::endl
             << " 2. name: " << page->name;
         throw std::invalid_argument(msg.str());
      }

      _quickControlsPagesIndex.insert_or_assign(page->id, page.get());
      _quickControlsPages.emplace_back(std::move(page));
   }

   quickControlsPagesChanged();
   quickControlsSetSelectedPage(firstPageId);
}

void PluginHost::quickControlsSetSelectedPage(clap_id pageId) {
   checkForMainThread();
   if (pageId == _quickControlsSelectedPage)
      return;

   if (pageId != CLAP_INVALID_ID) {
      auto it = _quickControlsPagesIndex.find(pageId);
      if (it == _quickControlsPagesIndex.end()) {
         std::ostringstream msg;
         msg << "quick control page_id " << pageId << " not found";
         throw std::invalid_argument(msg.str());
      }
   }

   _quickControlsSelectedPage = pageId;
   quickControlsSelectedPageChanged();
}

void PluginHost::setQuickControlsSelectedPageByHost(clap_id page_id) {
   checkForMainThread();
   Q_ASSERT(page_id != CLAP_INVALID_ID);

   checkForMainThread();

   _quickControlsSelectedPage = page_id;
}

void PluginHost::clapQuickControlsChanged(const clap_host *host) {
   checkForMainThread();

   auto h = fromHost(host);
   if (!h->_pluginQuickControls) {
      std::ostringstream msg;
      msg << "Plugin called clap_host_quick_controls.changed() but does not provide "
             "clap_plugin_quick_controls";
      throw std::logic_error(msg.str());
   }

   h->scanQuickControls();
}

bool PluginHost::loadNativePluginPreset(const std::string &path) {
   checkForMainThread();

   if (!_pluginPresetLoad)
      return false;

   if (!_pluginPresetLoad->from_file)
      throw std::logic_error("clap_plugin_preset_load does not implement load_from_file");

   return _pluginPresetLoad->from_file(_plugin, path.c_str());
}

void PluginHost::clapStateMarkDirty(const clap_host *host) {
   checkForMainThread();

   auto h = fromHost(host);

   if (!h->_pluginState || !h->_pluginState->save || !h->_pluginState->load)
      throw std::logic_error("Plugin called clap_host_state.set_dirty() but the host does not "
                             "provide a complete clap_plugin_state interface.");

   h->_stateIsDirty = true;
}

void PluginHost::setPluginState(PluginState state) {
   switch (state) {
   case Inactive:
      Q_ASSERT(_state == ActiveAndReadyToDeactivate);
      break;

   case InactiveWithError:
      Q_ASSERT(_state == Inactive);
      break;

   case ActiveAndSleeping:
      Q_ASSERT(_state == Inactive || _state == ActiveAndProcessing);
      break;

   case ActiveAndProcessing:
      Q_ASSERT(_state == ActiveAndSleeping);
      break;

   case ActiveWithError:
      Q_ASSERT(_state == ActiveAndProcessing);
      break;

   case ActiveAndReadyToDeactivate:
      Q_ASSERT(_state == ActiveAndProcessing || _state == ActiveAndSleeping ||
               _state == ActiveWithError);
      break;

   default:
      std::terminate();
   }

   _state = state;
}

bool PluginHost::isPluginActive() const {
   switch (_state) {
   case Inactive:
   case InactiveWithError:
      return false;
   default:
      return true;
   }
}

bool PluginHost::isPluginProcessing() const { return _state == ActiveAndProcessing; }

bool PluginHost::isPluginSleeping() const { return _state == ActiveAndSleeping; }

QString PluginHost::paramValueToText(clap_id paramId, double value) {
   std::array<char, 256> buffer;

   if (!canUsePluginParams())
      return "-";

   if (_pluginParams->value_to_text(_plugin, paramId, value, buffer.data(), buffer.size()))
      return buffer.data();

   return QString::number(value);
}

bool PluginHost::canUsePluginParams() const noexcept {
   return _pluginParams && _pluginParams->count && _pluginParams->flush &&
          _pluginParams->get_info && _pluginParams->get_value && _pluginParams->text_to_value &&
          _pluginParams->value_to_text;
}

bool PluginHost::canUsePluginGui() const noexcept {
   return _pluginGui && _pluginGui->create && _pluginGui->destroy && _pluginGui->can_resize &&
          _pluginGui->get_size && _pluginGui->adjust_size && _pluginGui->set_size &&
          _pluginGui->set_scale && _pluginGui->hide && _pluginGui->show &&
          _pluginGui->suggest_title && _pluginGui->is_api_supported;
}
