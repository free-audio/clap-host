#pragma once

#include "audio-settings.hh"
#include "midi-settings.hh"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

class Settings {
public:
   Settings();

   void load(QSettings &settings);
   void save(QSettings &settings) const;

   AudioSettings &audioSettings() { return _audioSettings; }
   MidiSettings &midiSettings() { return _midiSettings; }

private:
   AudioSettings _audioSettings;
   MidiSettings _midiSettings;
};
