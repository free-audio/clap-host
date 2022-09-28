#pragma once

#include <QDialog>

class Settings;

class TweaksDialog : public QDialog {
public:
   TweaksDialog(Settings &settings, QWidget *parent = nullptr);
   ~TweaksDialog() override = default;
};
