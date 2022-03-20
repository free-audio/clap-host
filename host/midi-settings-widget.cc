#include <iostream>

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <RtMidi.h>

#include "application.hh"
#include "engine.hh"
#include "midi-settings-widget.hh"
#include "midi-settings.hh"

MidiSettingsWidget::MidiSettingsWidget(MidiSettings &midiSettings, QWidget *parent)
   : super(parent), _midiSettings(midiSettings) {
   _apiChooser = new QComboBox(this);
   _deviceChooser = new QComboBox(this);

   auto layout = new QGridLayout(this);
   layout->addWidget(new QLabel(tr("API")), 0, 0);
   layout->addWidget(new QLabel(tr("Device")), 1, 0);

   layout->addWidget(_apiChooser, 0, 1);
   layout->addWidget(_deviceChooser, 1, 1);

   QGroupBox *groupBox = new QGroupBox(this);
   groupBox->setLayout(layout);
   groupBox->setTitle(tr("Audio"));

   QLayout *groupLayout = new QVBoxLayout();
   groupLayout->addWidget(groupBox);
   setLayout(groupLayout);

   initApiList();
   refresh();

   connect(
      _apiChooser, &QComboBox::currentIndexChanged, this, &MidiSettingsWidget::selectedApiChanged);
   connect(_deviceChooser,
           &QComboBox::currentIndexChanged,
           this,
           &MidiSettingsWidget::selectedDeviceChanged);
}

MidiSettingsWidget::~MidiSettingsWidget() = default;

void MidiSettingsWidget::initApiList() {
   _apiChooser->clear();

   auto selectedApi = _midiSettings.deviceReference()._api.toStdString();

   std::vector<RtMidi::Api> APIs;
   RtMidi::getCompiledApi(APIs);
   for (const auto &api : APIs) {
      _apiChooser->addItem(QString::fromStdString(RtMidi::getApiDisplayName(api)));
      if (selectedApi == RtMidi::getApiName(api))
         _apiChooser->setCurrentIndex(_apiChooser->count() - 1);
   }
}

void MidiSettingsWidget::refresh() {
   _midiIn.reset(); // make sure we delete the old one first
   _midiIn = std::make_unique<RtMidiIn>(getSelectedMidiApi(), "clap-host-settings");

   _isRefreshingDeviceList = true;
   updateDeviceList();
   _isRefreshingDeviceList = false;
}

void MidiSettingsWidget::updateDeviceList() {
   _deviceChooser->clear();

   auto deviceCount = _midiIn->getPortCount();
   bool deviceFound = false;

   // Populate the choices
   for (int i = 0; i < deviceCount; ++i) {
      QString name = QString::fromStdString(_midiIn->getPortName(i));
      _deviceChooser->addItem(name);

      if (!deviceFound && _midiSettings.deviceReference()._name == name) {
         _deviceChooser->setCurrentIndex(i);
         deviceFound = true;
      }
   }

   if (!deviceFound)
      _deviceChooser->setCurrentIndex(0);
}

RtMidi::Api MidiSettingsWidget::getSelectedMidiApi() const {
   std::vector<RtMidi::Api> APIs;
   RtMidi::getCompiledApi(APIs);

   const auto index = _apiChooser->currentIndex();
   if (index >= APIs.size())
      return RtMidi::RTMIDI_DUMMY;
   return APIs[index];
}

void MidiSettingsWidget::selectedApiChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   refresh();
   saveSettings();
}

void MidiSettingsWidget::selectedDeviceChanged(int index) {
   if (_isRefreshingDeviceList)
      return;

   saveSettings();
}

void MidiSettingsWidget::saveSettings() {
   if (_isRefreshingDeviceList)
      return;

   int index = _deviceChooser->currentIndex();
   auto portName = _midiIn->getPortName(index);

   DeviceReference ref;
   auto apiName = RtMidi::getApiName(getSelectedMidiApi());
   ref._api = QString::fromStdString(apiName);
   ref._index = index;
   ref._name = QString::fromStdString(portName);
   _midiSettings.setDeviceReference(ref);
}
