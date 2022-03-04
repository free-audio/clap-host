#include <iostream>

#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>

#include <RtMidi.h>

#include "application.hh"
#include "engine.hh"
#include "midi-settings-widget.hh"
#include "midi-settings.hh"

MidiSettingsWidget::MidiSettingsWidget(MidiSettings &midiSettings) : _midiSettings(midiSettings) {
   auto layout = new QVBoxLayout(this);

   auto deviceComboBox = new QComboBox;
   bool deviceFound = false;

   auto &app = Application::instance();
   auto engine = app.engine();
   auto midiIn = engine ? engine->midiIn() : nullptr;

   auto deviceCount = midiIn ? midiIn->getPortCount() : 0;
   int inputIndex = 0;

   if (deviceCount <= 0) {
      std::cerr << "warning: no midi device found!" << std::endl;
   }

   for (int i = 0; i < deviceCount; ++i) {
      auto name = QString::fromStdString(midiIn->getPortName(i));

      deviceComboBox->addItem(name);

      if (!deviceFound && _midiSettings.deviceReference()._index == i &&
          _midiSettings.deviceReference()._name == name) {
         deviceComboBox->setCurrentIndex(inputIndex);
         deviceFound = true;
         selectedDeviceChanged(inputIndex);
      }

      ++inputIndex;
   }

   // try to find the device just by its name.
   inputIndex = 0;
   for (int i = 0; !deviceFound && i < deviceCount; ++i) {
      auto name = QString::fromStdString(midiIn->getPortName(i));

      if (_midiSettings.deviceReference()._name == name) {
         deviceComboBox->setCurrentIndex(inputIndex);
         deviceFound = true;
         selectedDeviceChanged(inputIndex);
      }

      ++inputIndex;
   }

   connect(
      deviceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(selectedDeviceChanged(int)));

   layout->addWidget(deviceComboBox);

   QGroupBox *groupBox = new QGroupBox;
   groupBox->setLayout(layout);
   groupBox->setTitle(tr("MIDI"));

   QVBoxLayout *groupLayout = new QVBoxLayout;
   groupLayout->addWidget(groupBox);
   setLayout(groupLayout);
}

void MidiSettingsWidget::selectedDeviceChanged(int index) {
   auto &app = Application::instance();
   auto engine = app.engine();
   auto midiIn = engine ? engine->midiIn() : nullptr;

   int inputIndex = 0;
   auto deviceCount = midiIn ? midiIn->getPortCount() : 0;
   for (int i = 0; i < deviceCount; ++i) {
      auto name = QString::fromStdString(midiIn->getPortName(i));

      if (inputIndex != index) {
         ++inputIndex;
         continue;
      }

      DeviceReference ref;
      ref._index = i;
      ref._name = name;
      _midiSettings.setDeviceReference(ref);
      break;
   }
}
