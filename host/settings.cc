#include "settings.hh"

Settings::Settings() {}

void Settings::load(QSettings &settings) {
   _audioSettings.load(settings);
   _midiSettings.load(settings);
   _pluginHostSettings.load(settings);
}

void Settings::save(QSettings &settings) const {
   _audioSettings.save(settings);
   _midiSettings.save(settings);
   _pluginHostSettings.save(settings);
}
