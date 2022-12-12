#include <QSettings>

#include "plugin-host-settings.hh"

static const char SHOULD_PROVIDE_COOKIE_KEY[] = "PluginHost/ShouldProvideCookie";

PluginHostSettings::PluginHostSettings() {}

void PluginHostSettings::load(QSettings &settings) {
   _shouldProvideCookie = settings.value(SHOULD_PROVIDE_COOKIE_KEY).toBool();
}

void PluginHostSettings::save(QSettings &settings) const {
   settings.setValue(SHOULD_PROVIDE_COOKIE_KEY, _shouldProvideCookie);
}
