﻿#include <iostream>

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

   _apiChooser = new QComboBox(this);
   _deviceChooser = new QComboBox(this);
   _sampleRateChooser = new QComboBox(this);
   _bufferSizeChooser = new QComboBox(this);

   auto layout = new QGridLayout(this);
   layout->addWidget(new QLabel(tr("API")), 0, 0);
   layout->addWidget(new QLabel(tr("Device")), 1, 0);
   layout->addWidget(new QLabel(tr("Sample rate")), 2, 0);
   layout->addWidget(new QLabel(tr("Buffer size")), 3, 0);

   layout->addWidget(_apiChooser, 0, 1);
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
      _apiChooser, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::selectedApiChanged);

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

void AudioSettingsWidget::initApiList() {
   _apiChooser->clear();

   auto selectedApi = _audioSettings.deviceReference()._api.toStdString();

   std::vector<RtAudio::Api> APIs;
   RtAudio::getCompiledApi(APIs);
   for (const auto &api : APIs) {
      _apiChooser->addItem(QString::fromStdString(RtAudio::getApiDisplayName(api)));
      if (selectedApi == RtAudio::getApiName(api))
         _apiChooser->setCurrentIndex(_apiChooser->count() - 1);
   }
}

void AudioSettingsWidget::refresh() {
   _audio.reset(); // make sure we delete the old one first
   _audio = std::make_unique<RtAudio>(getSelectedAudioApi());

   _isRefreshingDeviceList = true;

   updateDeviceList();
   updateSampleRateList();
   updateBufferSizeList();

   _isRefreshingDeviceList = false;
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
   auto deviceIds = _audio->getDeviceIds();
   auto info = _audio->getDeviceInfo(deviceIds[_deviceChooser->currentIndex()]);
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
   auto deviceIds   = _audio->getDeviceIds();

   // Populate the choices
   for (int i = 0; i < deviceCount; ++i) {
      auto deviceId = deviceIds[i];
      auto deviceInfo = _audio->getDeviceInfo(deviceId);
      QString name = QString::fromStdString(deviceInfo.name);
      _deviceChooser->addItem(name);

      if (!deviceFound && _audioSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         deviceFound = true;
      }
   }

   if (!deviceFound)
      _deviceChooser->setCurrentIndex(0);
}

RtAudio::Api AudioSettingsWidget::getSelectedAudioApi() const {
   std::vector<RtAudio::Api> APIs;
   RtAudio::getCompiledApi(APIs);

   const auto index = _apiChooser->currentIndex();
   if (index >= APIs.size())
      return RtAudio::RTAUDIO_DUMMY;
   return APIs[index];
}

void AudioSettingsWidget::selectedApiChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   refresh();
   saveSettings();
}

void AudioSettingsWidget::selectedDeviceChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   updateBufferSizeList();
   updateSampleRateList();
   saveSettings();
}

void AudioSettingsWidget::selectedSampleRateChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   saveSettings();
}

void AudioSettingsWidget::selectedBufferSizeChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   saveSettings();
}

void AudioSettingsWidget::saveSettings() {
   if (_isRefreshingDeviceList)
      return;

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