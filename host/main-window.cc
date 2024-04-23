#include <iostream>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QToolBar>
#include <QWindow>
#include <QUrl>

#include "about-dialog.hh"
#include "application.hh"
#include "engine.hh"
#include "main-window.hh"
#include "plugin-host.hh"
#include "plugin-parameters-widget.hh"
#include "plugin-quick-controls-widget.hh"
#include "settings-dialog.hh"
#include "settings.hh"
#include "tweaks-dialog.hh"

MainWindow::MainWindow(Application &app)
   : super(nullptr), _application(app), _pluginViewWindow(new QWindow()),
     _pluginViewWidget(QWidget::createWindowContainer(_pluginViewWindow)) {

   createMenu();

   setCentralWidget(_pluginViewWidget);
   _pluginViewWidget->show();
   _pluginViewWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
   setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

   auto &pluginHost = app.engine()->pluginHost();

   _pluginParametersWidget = new PluginParametersWidget(nullptr, pluginHost);
   _pluginRemoteControlsWidget = new PluginQuickControlsWidget(nullptr, pluginHost);
}

MainWindow::~MainWindow() {}

void MainWindow::createMenu() {
   QMenuBar *menuBar = new QMenuBar(this);
   setMenuBar(menuBar);

   QMenu *fileMenu = menuBar->addMenu(tr("File"));
   // TODO: fileMenu->addAction(tr("Load plugin"));

   _loadPluginPresetAction = fileMenu->addAction(tr("Load Native Plugin Preset"));
   connect(_loadPluginPresetAction,
           &QAction::triggered,
           this,
           &MainWindow::loadNativePluginPreset);
   fileMenu->addSeparator();
   connect(fileMenu->addAction(tr("Settings")),
           &QAction::triggered,
           this,
           &MainWindow::showSettingsDialog);
   fileMenu->addSeparator();
   connect(fileMenu->addAction(tr("Quit")),
           &QAction::triggered,
           QApplication::instance(),
           &Application::quit);

   auto windowsMenu = menuBar->addMenu("Windows");

   _showPluginParametersAction = windowsMenu->addAction(tr("Show Parameters"));
   connect(_showPluginParametersAction,
           &QAction::triggered,
           this,
           &MainWindow::showPluginParametersWindow);

   _showPluginQuickControlsAction = windowsMenu->addAction(tr("Show Quick Controls"));
   connect(_showPluginQuickControlsAction,
           &QAction::triggered,
           this,
           &MainWindow::showPluginQuickControlsWindow);
   menuBar->addSeparator();
   connect(windowsMenu->addAction(tr("Tweaks...")), &QAction::triggered, [this] {
      TweaksDialog dialog(_application.settings(), this);
      dialog.exec();
   });
   menuBar->addSeparator();

   _togglePluginWindowVisibilityAction = windowsMenu->addAction(tr("Toggle Plugin Window Visibility"));
   connect(_togglePluginWindowVisibilityAction,
           &QAction::triggered,
           this,
           &MainWindow::togglePluginWindowVisibility);

   _recreatePluginWindowAction = windowsMenu->addAction(tr("Recreate Plugin Window"));
   connect(_recreatePluginWindowAction,
           &QAction::triggered,
           this,
           &MainWindow::recreatePluginWindow);

   QMenu *helpMenu = menuBar->addMenu(tr("Help"));
   connect(
      helpMenu->addAction(tr("About")), &QAction::triggered, this, &MainWindow::showAboutDialog);

   updatePluginMenuItems();

   assert(_application.engine());
   connect(&_application.engine()->pluginHost(), &PluginHost::pluginLoadedChanged, this, &MainWindow::updatePluginMenuItems);
}

void MainWindow::updatePluginMenuItems(bool const pluginLoaded /* = false */ ) {
   _loadPluginPresetAction->setEnabled(pluginLoaded);
   _showPluginParametersAction->setEnabled(pluginLoaded);
   _showPluginQuickControlsAction->setEnabled(pluginLoaded);
   _togglePluginWindowVisibilityAction->setEnabled(pluginLoaded);
   _recreatePluginWindowAction->setEnabled(pluginLoaded);
}

void MainWindow::showSettingsDialog() {
   SettingsDialog dialog(Application::instance().settings(), this);
   dialog.exec();
}

void MainWindow::showPluginParametersWindow() { _pluginParametersWidget->show(); }
void MainWindow::showPluginQuickControlsWindow() { _pluginRemoteControlsWidget->show(); }

WId MainWindow::getEmbedWindowId() { return _pluginViewWidget->winId(); }

static bool wantsLogicalSize() noexcept {
#ifdef Q_OS_MACOS
   return true;
#else
   return false;
#endif
}

void MainWindow::resizePluginView(int width, int height) {
   auto ratio = wantsLogicalSize() ? 1 : _pluginViewWidget->devicePixelRatio();
   auto sw = width / ratio;
   auto sh = height / ratio;
   _pluginViewWidget->setFixedSize(sw, sh);
   _pluginViewWidget->show();
   adjustSize();
}

void MainWindow::loadNativePluginPreset() {
   auto file = QFileDialog::getOpenFileName(this, tr("Load Plugin Native Preset"));
   if (file.isEmpty())
      return;

   _application.engine()->pluginHost().loadNativePluginPreset(file.toStdString());
}

void MainWindow::togglePluginWindowVisibility() {
   bool isVisible = !_pluginViewWidget->isVisible();
   _pluginViewWidget->setVisible(isVisible);
   _application.engine()->pluginHost().setPluginWindowVisibility(isVisible);
}

void MainWindow::recreatePluginWindow() {
   _application.engine()->pluginHost().setParentWindow(getEmbedWindowId());
}

void MainWindow::showAboutDialog() {
   AboutDialog dialog(this);
   dialog.exec();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
   Engine *engine = _application.engine();

   for (int i = 0; i < 8; i++) {
#ifdef _WIN32
      if (0 == InterlockedCompareExchange((LONG volatile *) &engine->keyboardNoteData[i], event->key(), 0)) {
#else
      if (0 == __sync_val_compare_and_swap(&engine->keyboardNoteData[i], 0, event->key())) {
#endif
         break;
      }
   }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
   Engine *engine = _application.engine();

   for (int i = 0; i < 8; i++) {
#ifdef _WIN32
      if (0 == InterlockedCompareExchange((LONG volatile *) &engine->keyboardNoteData[i], event->key(), 0)) {
#else
      if (0 == __sync_val_compare_and_swap(&engine->keyboardNoteData[i], 0, 0x8000 | event->key())) {
#endif
         break;
      }
   }
}

void MainWindow::closeEvent(QCloseEvent *event) {
   super::closeEvent(event);
   qApp->quit();
}
