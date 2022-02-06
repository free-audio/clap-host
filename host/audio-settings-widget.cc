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

AudioSettingsWidget::AudioSettingsWidget(AudioSettings &audioSettings, QWidget *parent)
   : QWidget(parent), _audioSettings(audioSettings) {

   _apiWidget = new QComboBox(this);
   _deviceChooser = new QComboBox(this);
   _sampleRateChooser = new QComboBox(this);
   _bufferSizeChooser = new QComboBox(this);

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

   initApiList();
   refresh();

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
}

AudioSettingsWidget::~AudioSettingsWidget() = default;

void AudioSettingsWidget::refresh() {
   _audio.reset(); // make sure we delete the old one first
   _audio = std::make_unique<RtAudio>(getSelectedAudioApi());

   updateDeviceList();
   updateSampleRateList();
   updateBufferSizeList();
}

void AudioSettingsWidget::initApiList() {
   _apiWidget->clear();

   auto selectedApi = _audioSettings.deviceReference()._api.toStdString();

   std::vector<RtAudio::Api> APIs;
   RtAudio::getCompiledApi(APIs);
   for (const auto &api : APIs) {
      _apiWidget->addItem(QString::fromStdString(RtAudio::getApiDisplayName(api)));
      if (selectedApi == RtAudio::getApiName(api))
         _apiWidget->setCurrentIndex(_apiWidget->count() - 1);
   }
}

void AudioSettingsWidget::updateBufferSizeList() {
   _bufferSizeChooser->clear();

   static const std::vector<int> BUFFER_SIZES = {64, 96, 128, 192, 256, 384, 512, 768, 1024};

   bool didSelectBufferSize = false;
   for (size_t i = 0; i < BUFFER_SIZES.size(); ++i) {
      int bs = BUFFER_SIZES[i];
      _bufferSizeChooser->addItem(QString::number(bs));
      if (bs == _audioSettings.bufferSize()) {
         _bufferSizeChooser->setCurrentIndex(i);
         didSelectBufferSize = true;
      }
   }

   if (!didSelectBufferSize)
      _bufferSizeChooser->setCurrentIndex(4);
}

void AudioSettingsWidget::updateSampleRateList() {
   _sampleRateChooser->clear();

   bool didSelectSampleRate = false;
   auto info = _audio->getDeviceInfo(_deviceChooser->currentIndex());
   for (size_t i = 0; i < info.sampleRates.size(); ++i) {
      int sr = info.sampleRates[i];
      _sampleRateChooser->addItem(QString::number(sr));
      if (sr == _audioSettings.sampleRate()) {
         _sampleRateChooser->setCurrentIndex(i);
         didSelectSampleRate = true;
      }
   }

   if (didSelectSampleRate)
      return;

   for (size_t i = 0; i < info.sampleRates.size(); ++i)
      if (info.sampleRates[i] == info.preferredSampleRate)
         _sampleRateChooser->setCurrentIndex(i);
}

void AudioSettingsWidget::updateDeviceList() {
   _deviceChooser->clear();

   auto deviceCount = _audio->getDeviceCount();
   bool deviceFound = false;

   // Populate the choices
   for (int i = 0; i < deviceCount; ++i) {
      auto deviceInfo = _audio->getDeviceInfo(i);
      QString name = QString::fromStdString(deviceInfo.name);
      _deviceChooser->addItem(name);

      if (!deviceFound && _audioSettings.deviceReference()._index == i &&
          _audioSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         deviceFound = true;
      }
   }

   if (deviceFound)
      return;

   // try to find the device just by its name.
   for (int i = 0; i < deviceCount; ++i) {
      auto deviceInfo = _audio->getDeviceInfo(i);
      QString name = QString::fromStdString(deviceInfo.name);

      if (_audioSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         return;
      }
   }

   // Device was not found
   _deviceChooser->setCurrentIndex(0);
}

RtAudio::Api AudioSettingsWidget::getSelectedAudioApi() const {
   std::vector<RtAudio::Api> APIs;
   RtAudio::getCompiledApi(APIs);

   const auto index = _apiWidget->currentIndex();
   if (index >= APIs.size())
      return RtAudio::RTAUDIO_DUMMY;
   return APIs[index];
}

void AudioSettingsWidget::selectedApiChanged(int index) {
   refresh();
   saveSettings();
}

void AudioSettingsWidget::selectedDeviceChanged(int index) {
   updateBufferSizeList();
   updateSampleRateList();
   saveSettings();
}

void AudioSettingsWidget::selectedSampleRateChanged(int index) { saveSettings(); }

void AudioSettingsWidget::selectedBufferSizeChanged(int index) { saveSettings(); }

void AudioSettingsWidget::saveSettings() {
   int index = _deviceChooser->currentIndex();
   auto deviceInfo = _audio->getDeviceInfo(index);

   DeviceReference ref;
   auto apiName = RtAudio::getApiName(getSelectedAudioApi());
   ref._api = QString::fromStdString(apiName);
   ref._index = index;
   ref._name = QString::fromStdString(deviceInfo.name);
   _audioSettings.setDeviceReference(ref);
   _audioSettings.setSampleRate(_sampleRateChooser->currentText().toInt());
   _audioSettings.setBufferSize(_bufferSizeChooser->currentText().toInt());
}