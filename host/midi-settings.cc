#include <QSettings>

#include "midi-settings.hh"

static const char DEVICE_NAME_KEY[] = "Midi/DeviceName";
static const char DEVICE_INDEX_KEY[] = "Midi/DeviceIndex";

MidiSettings::MidiSettings() {}

void MidiSettings::load(QSettings &settings) {
   _deviceReference._name = settings.value(DEVICE_NAME_KEY).toString();
   _deviceReference._index = settings.value(DEVICE_INDEX_KEY).toInt();
}

void MidiSettings::save(QSettings &settings) const {
   settings.setValue(DEVICE_NAME_KEY, _deviceReference._name);
   settings.setValue(DEVICE_INDEX_KEY, _deviceReference._index);
}
