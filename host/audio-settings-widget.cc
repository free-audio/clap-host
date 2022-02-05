#include <iostream>

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include "application.hh"
#include "audio-settings-widget.hh"
#include "audio-settings.hh"
#include "engine.hh"

static const std::vector<int> BUFFER_SIZES = {32, 48, 64, 96, 128, 192, 256, 384, 512};

AudioSettingsWidget::AudioSettingsWidget(AudioSettings &audioSettings)
   : _audioSettings(audioSettings) {

   _apiWidget = new QComboBox(this);
   _deviceChooser = new QComboBox(this);
   _sampleRateChooser = new QComboBox(this);
   _bufferSizeChooser = new QComboBox(this);

   connect(
      _apiWidget, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::selectedApiChanged);

   connect(_deviceChooser,
           &QComboBox::currentIndexChanged,
           this,
           &AudioSettingsWidget::selectedDeviceChanged);

   connect(_sampleRateChooser,
           &QComboBox::currentIndexChanged,
           this,
           &AudioSettingsWidget::selectedSampleRateChanged);

   connect(_bufferSizeChooser,
           &QComboBox::currentIndexChanged,
           this,
           &AudioSettingsWidget::selectedBufferSizeChanged);

   auto layout = new QGridLayout(this);
   layout->addWidget(new QLabel(tr("API")), 0, 0);
   layout->addWidget(new QLabel(tr("Device")), 1, 0);
   layout->addWidget(new QLabel(tr("Sample rate")), 2, 0);
   layout->addWidget(new QLabel(tr("Buffer size")), 3, 0);

   layout->addWidget(_apiWidget, 0, 1);
   layout->addWidget(_deviceChooser, 1, 1);
   layout->addWidget(_sampleRateChooser, 2, 1);
   layout->addWidget(_bufferSizeChooser, 3, 1);

   QGroupBox *groupBox = new QGroupBox(this);
   groupBox->setLayout(layout);
   groupBox->setTitle(tr("Audio"));

   QLayout *groupLayout = new QVBoxLayout();
   groupLayout->addWidget(groupBox);
   setLayout(groupLayout);

   updateApiList();
   updateDeviceList();
   updateSampleRateList();
   updateBufferSizeList();
}

void AudioSettingsWidget::updateApiList() {
   _apiWidget->clear();

   std::vector<RtAudio::Api> APIs;
   RtAudio::getCompiledApi(APIs);
   for (const auto &api : APIs) {
      _apiWidget->addItem(QString::fromStdString(RtAudio::getApiDisplayName(api)));
   }
}

void AudioSettingsWidget::updateBufferSizeList() {
   _bufferSizeChooser->clear();

   for (size_t i = 0; i < BUFFER_SIZES.size(); ++i) {
      int bs = BUFFER_SIZES[i];
      _bufferSizeChooser->addItem(QString::number(bs));
      if (bs == _audioSettings.bufferSize()) {
         _bufferSizeChooser->setCurrentIndex(i);
         selectedBufferSizeChanged(i);
      }
   }
}

void AudioSettingsWidget::updateSampleRateList() {
   static const std::vector<int> SAMPLE_RATES = {
      44100,
      48000,
      88200,
      96000,
      176400,
      192000,
   };

   _sampleRateChooser->clear();

   for (size_t i = 0; i < SAMPLE_RATES.size(); ++i) {
      int sr = SAMPLE_RATES[i];
      _sampleRateChooser->addItem(QString::number(sr));
      if (sr == _audioSettings.sampleRate()) {
         _sampleRateChooser->setCurrentIndex(i);
         selectedSampleRateChanged(i);
      }
   }
}

void AudioSettingsWidget::updateDeviceList() {
   _deviceChooser->clear();

   auto engine = Application::instance().engine();
   auto audio = engine->audio();

   auto deviceCount = audio->getDeviceCount();
   bool deviceFound = false;

   for (int i = 0; i < deviceCount; ++i) {
      auto deviceInfo = audio->getDeviceInfo(i);
      QString name = QString::fromStdString(deviceInfo.name);
      _deviceChooser->addItem(name);

      if (!deviceFound && _audioSettings.deviceReference()._index == i &&
          _audioSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         deviceFound = true;
         selectedDeviceChanged(i);
      }
   }

   // try to find the device just by its name.
   for (int i = 0; !deviceFound && i < deviceCount; ++i) {
      auto deviceInfo = audio->getDeviceInfo(i);
      QString name = QString::fromStdString(deviceInfo.name);

      if (_audioSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         deviceFound = true;
         selectedDeviceChanged(i);
      }
   }

   if (!deviceFound) {
      _deviceChooser->setCurrentIndex(0);
      selectedDeviceChanged(0);
   }
}

void AudioSettingsWidget::selectedApiChanged(int index) {}

void AudioSettingsWidget::selectedDeviceChanged(int index) {
   auto engine = Application::instance().engine();
   auto audio = engine->audio();
   auto deviceInfo = audio->getDeviceInfo(index);

   DeviceReference ref;
   ref._index = index;
   ref._name = QString::fromStdString(deviceInfo.name);
   _audioSettings.setDeviceReference(ref);
}

void AudioSettingsWidget::selectedSampleRateChanged(int index) {
   _audioSettings.setSampleRate(_sampleRateChooser->itemText(index).toInt());
}

void AudioSettingsWidget::selectedBufferSizeChanged(int index) {
   _audioSettings.setBufferSize(_bufferSizeChooser->itemText(index).toInt());
}
