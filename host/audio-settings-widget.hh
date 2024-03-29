﻿#pragma once

#include <QWidget>

#include <RtAudio.h>

class AudioSettings;

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

class AudioSettingsWidget final : public QWidget {
   Q_OBJECT
   using super = QWidget;

public:
   explicit AudioSettingsWidget(AudioSettings &audioSettings, QWidget *parent = nullptr);
   ~AudioSettingsWidget() override;

private:
   void refresh();
   void initApiList();
   void updateSampleRateList();
   void updateBufferSizeList();
   void updateDeviceList();

   void selectedApiChanged(int index);
   void selectedDeviceChanged(int index);
   void selectedSampleRateChanged(int index);
   void selectedBufferSizeChanged(int index);

   RtAudio::Api getSelectedAudioApi() const;

   void saveSettings();

   AudioSettings &_audioSettings;
   QComboBox *_apiChooser = nullptr;
   QComboBox *_deviceChooser = nullptr;
   QComboBox *_sampleRateChooser = nullptr;
   QComboBox *_bufferSizeChooser = nullptr;
   std::unique_ptr<RtAudio> _audio;
   bool _isRefreshingDeviceList = false;
};
