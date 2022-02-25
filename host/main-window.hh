#pragma once

#include <QMainWindow>

class Application;
class SettingsDialog;
class PluginParametersWidget;
class PluginQuickControlsWidget;

class MainWindow : public QMainWindow {
   using super = QMainWindow;

   Q_OBJECT

public:
   explicit MainWindow(Application &app);
   ~MainWindow();

   WId getEmbedWindowId();

public:
   void loadNativePluginPreset();
   void showSettingsDialog();
   void showPluginParametersWindow();
   void showPluginQuickControlsWindow();
   void resizePluginView(int width, int height);

   void showPluginWindow() { _pluginViewWidget->show(); }

   void hidePluginWindow() { _pluginViewWidget->hide(); }

protected:
   void closeEvent(QCloseEvent *event) override;

private:
   void createMenu();

   void togglePluginWindowVisibility();
   void recreatePluginWindow();
   void showAboutDialog();

   Application &_application;
   QWindow *_pluginViewWindow = nullptr;
   QWidget *_pluginViewWidget = nullptr;

   PluginParametersWidget *_pluginParametersWidget = nullptr;
   PluginQuickControlsWidget *_pluginQuickControlsWidget = nullptr;
};
