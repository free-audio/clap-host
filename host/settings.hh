#pragma once

#include "audio-settings.hh"
#include "midi-settings.hh"
#include "plugin-host-settings.hh"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

class Settings {
public:
   Settings();

   void load(QSettings &settings);
   void save(QSettings &settings) const;

   auto &audioSettings() { return _audioSettings; }
   auto &midiSettings() { return _midiSettings; }
   auto &pluginHostSettings() { return _pluginHostSettings; }

private:
   AudioSettings _audioSettings;
   MidiSettings _midiSettings;
   PluginHostSettings _pluginHostSettings;
};
