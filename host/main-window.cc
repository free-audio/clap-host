#include <iostream>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QToolBar>
#include <QWindow>

#include "about-dialog.hh"
#include "application.hh"
#include "engine.hh"
#include "main-window.hh"
#include "plugin-host.hh"
#include "plugin-parameters-widget.hh"
#include "plugin-quick-controls-widget.hh"
#include "settings-dialog.hh"
#include "settings.hh"

MainWindow::MainWindow(Application &app)
   : QMainWindow(nullptr), _application(app),
     _pluginViewWindow(new QWindow()),
     _pluginViewWidget(QWidget::createWindowContainer(_pluginViewWindow)) {

   createMenu();

   setCentralWidget(_pluginViewWidget);
   _pluginViewWidget->show();
   _pluginViewWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
   setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
   setMinimumSize(100, 75);

   auto &pluginHost = app.engine()->pluginHost();

   _pluginParametersWindow = new QMainWindow(this);
   _pluginParametersWidget = new PluginParametersWidget(_pluginParametersWindow, pluginHost);
   _pluginParametersWindow->setCentralWidget(_pluginParametersWidget);

   _pluginQuickControlsWindow = new QMainWindow(this);
   _pluginQuickControlsWidget =
      new PluginQuickControlsWidget(_pluginQuickControlsWindow, pluginHost);
   _pluginQuickControlsWindow->setCentralWidget(_pluginQuickControlsWidget);
}

MainWindow::~MainWindow() {}

void MainWindow::createMenu() {
   QMenuBar *menuBar = new QMenuBar(this);
   setMenuBar(menuBar);

   QMenu *fileMenu = menuBar->addMenu(tr("File"));
   // TODO: fileMenu->addAction(tr("Load plugin"));
   connect(fileMenu->addAction(tr("Load Native Plugin Preset")),
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
   connect(windowsMenu->addAction(tr("Show Parameters")),
           &QAction::triggered,
           this,
           &MainWindow::showPluginParametersWindow);
   connect(windowsMenu->addAction(tr("Show Quick Controls")),
           &QAction::triggered,
           this,
           &MainWindow::showPluginQuickControlsWindow);
   menuBar->addSeparator();
   connect(windowsMenu->addAction(tr("Toggle Plugin Window Visibility")),
           &QAction::triggered,
           this,
           &MainWindow::togglePluginWindowVisibility);
   connect(windowsMenu->addAction(tr("Recreate Plugin Window")),
           &QAction::triggered,
           this,
           &MainWindow::recreatePluginWindow);
   connect(windowsMenu->addAction(tr("Scale Plugin Window")),
           &QAction::triggered,
           this,
           &MainWindow::scalePluginWindow);

   QMenu *helpMenu = menuBar->addMenu(tr("Help"));
   connect(
      helpMenu->addAction(tr("About")), &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::showSettingsDialog() {
   SettingsDialog dialog(Application::instance().settings(), this);
   dialog.exec();
}

void MainWindow::showPluginParametersWindow() { _pluginParametersWindow->show(); }
void MainWindow::showPluginQuickControlsWindow() { _pluginQuickControlsWindow->show(); }

WId MainWindow::getEmbedWindowId() { return _pluginViewWidget->winId(); }

void MainWindow::resizePluginView(int width, int height) {
   auto ratio = _pluginViewWidget->devicePixelRatio();
   auto sw = width / ratio;
   auto sh = height / ratio;
   _pluginViewWidget->setMinimumSize(sw, sh);
   _pluginViewWidget->setMaximumSize(sw, sh);
   _pluginViewWidget->show();
   adjustSize();
}

void MainWindow::loadNativePluginPreset()
{
   auto file = QFileDialog::getOpenFileName(this, tr("Load Plugin Native Preset"));
   if (file.isEmpty())
      return;

   _application.engine()->pluginHost().loadNativePluginPreset(file.toStdString());
}

void MainWindow::togglePluginWindowVisibility()
{
   bool isVisible = !_pluginViewWidget->isVisible();
   _pluginViewWidget->setVisible(isVisible);
   _application.engine()->pluginHost().setPluginWindowVisibility(isVisible);
}

void MainWindow::recreatePluginWindow()
{
   _application.engine()->pluginHost().setParentWindow(getEmbedWindowId());
}

void MainWindow::scalePluginWindow()
{
   // TODO
}

void MainWindow::showAboutDialog()
{
   AboutDialog dialog(this);
   dialog.exec();
}