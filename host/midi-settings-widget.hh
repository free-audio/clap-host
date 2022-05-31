#pragma once

#include <QWidget>

#include <rtmidi/RtMidi.h>

class MidiSettings;

QT_BEGIN_NAMESPACE
class QComboBox;
QT_END_NAMESPACE

class MidiSettingsWidget final : public QWidget {
   Q_OBJECT
   using super = QWidget;

public:
   explicit MidiSettingsWidget(MidiSettings &midiSettings, QWidget *parent = nullptr);
   ~MidiSettingsWidget() override;

private:
   void refresh();
   void initApiList();
   void updateDeviceList();

   void selectedApiChanged(int index);
   void selectedDeviceChanged(int index);

   RtMidi::Api getSelectedMidiApi() const;

   void saveSettings();

   MidiSettings &_midiSettings;
   QComboBox *_apiChooser = nullptr;
   QComboBox *_deviceChooser = nullptr;
   std::unique_ptr<RtMidiIn> _midiIn;
   bool _isRefreshingDeviceList = false;
};
