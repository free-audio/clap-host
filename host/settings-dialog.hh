#pragma once

#include <QDialog>

class Settings;
class SettingsWidget;

class SettingsDialog : public QDialog {
public:
   SettingsDialog(Settings &settings, QWidget *parent = nullptr);
   ~SettingsDialog() override;

private:
   Settings &_settings;
   SettingsWidget *_settingsWidget = nullptr;
};
