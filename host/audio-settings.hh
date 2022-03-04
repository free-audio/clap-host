#pragma once

#include "device-reference.hh"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

class AudioSettings {
public:
   AudioSettings();

   void load(QSettings &settings);
   void save(QSettings &settings) const;

   int sampleRate() const { return _sampleRate; }
   void setSampleRate(int sampleRate) { _sampleRate = sampleRate; }

   void setDeviceReference(DeviceReference dr) { _deviceReference = dr; }
   const DeviceReference &deviceReference() const { return _deviceReference; }

   int bufferSize() const { return _bufferSize; }
   void setBufferSize(int bufferSize) { _bufferSize = bufferSize; }

private:
   DeviceReference _deviceReference;
   int _sampleRate = 44100;
   int _bufferSize = 128;
};
