#pragma once

#include <QWidget>

class AudioSettings;

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

class AudioSettingsWidget : public QWidget {
   Q_OBJECT
public:
   explicit AudioSettingsWidget(AudioSettings &audioSettings);

private:
   void updateApiList();
   void updateSampleRateList();
   void updateBufferSizeList();
   void updateDeviceList();

   void selectedApiChanged(int index);
   void selectedDeviceChanged(int index);
   void selectedSampleRateChanged(int index);
   void selectedBufferSizeChanged(int index);

private:
   AudioSettings &_audioSettings;
   QComboBox *_apiWidget = nullptr;
   QComboBox *_deviceChooser = nullptr;
   QComboBox *_sampleRateChooser = nullptr;
   QComboBox *_bufferSizeChooser = nullptr;
};
