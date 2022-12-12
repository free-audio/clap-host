#pragma once

#include <QObject>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

class PluginHostSettings {
public:
   PluginHostSettings();

   void load(QSettings &settings);
   void save(QSettings &settings) const;

   bool shouldProvideCookie() const { return _shouldProvideCookie; }
   void setShouldProvideCookie(bool enable) { _shouldProvideCookie = enable; }

private:
   bool _shouldProvideCookie = true;
};
