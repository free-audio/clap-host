#include "plugin-param.hh"
#include "plugin-host.hh"

PluginParam::PluginParam(PluginHost &pluginHost, const clap_param_info &info, double value)
   : QObject(&pluginHost), _info(info), _value(value) {}

void PluginParam::setValue(double v) {
   if (_value == v)
      return;

   _value = v;
   valueChanged();
}

void PluginParam::setModulation(double v) {
   if (_modulation == v)
      return;

   _modulation = v;
   modulatedValueChanged();
}

bool PluginParam::isValueValid(const double v) const {
   return _info.min_value <= v && v <= _info.max_value;
}

void PluginParam::printShortInfo(std::ostream &os) const {
   os << "id: " << _info.id << ", name: '" << _info.name << "', module: '" << _info.module << "'";
}

void PluginParam::printInfo(std::ostream &os) const {
   printShortInfo(os);
   os << ", min: " << _info.min_value << ", max: " << _info.max_value;
}

bool PluginParam::isInfoEqualTo(const clap_param_info &info) const {
   extern bool zeroOutParamCookies;
   bool cookiesSame = zeroOutParamCookies || (info.cookie == _info.cookie);
   return cookiesSame &&
      info.default_value == _info.default_value &&
      info.max_value == _info.max_value &&
      info.min_value == _info.min_value &&
      info.flags == _info.flags &&
      info.id == _info.id &&
      !::std::strncmp(info.name, _info.name, sizeof (info.name)) &&
      !::std::strncmp(info.module, _info.module, sizeof (info.module));
}

bool PluginParam::isInfoCriticallyDifferentTo(const clap_param_info &info) const {
   assert(_info.id == info.id);
   const uint32_t criticalFlags =
      CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_AUTOMATABLE_PER_NOTE_ID |
      CLAP_PARAM_IS_AUTOMATABLE_PER_KEY | CLAP_PARAM_IS_AUTOMATABLE_PER_CHANNEL |
      CLAP_PARAM_IS_AUTOMATABLE_PER_PORT | CLAP_PARAM_IS_MODULATABLE |
      CLAP_PARAM_IS_MODULATABLE_PER_NOTE_ID | CLAP_PARAM_IS_MODULATABLE_PER_KEY |
      CLAP_PARAM_IS_MODULATABLE_PER_CHANNEL | CLAP_PARAM_IS_MODULATABLE_PER_PORT |
      CLAP_PARAM_IS_READONLY | CLAP_PARAM_REQUIRES_PROCESS;
   return (_info.flags & criticalFlags) == (info.flags & criticalFlags) ||
          _info.min_value != _info.min_value || _info.max_value != _info.max_value;
}
