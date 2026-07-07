#include "Config.h"

Config &Config::instance()
{
  static Config inst;
  return inst;
}

void Config::load()
{
  _prefs.begin("device", true);
  data.wifiSSID = _prefs.getString("ssid", "");
  data.wifiPassword = _prefs.getString("pass", "");
  data.timezone = _prefs.getString("tz", "UTC0");
  data.city = _prefs.getString("city", "London");
  data.owmApiKey = _prefs.getString("owmkey", "");
  data.metricUnits = _prefs.getBool("metric", true);
  data.muted = _prefs.getBool("muted", false);
  _prefs.end();
}

void Config::save()
{
  _prefs.begin("device", false);
  _prefs.putString("ssid", data.wifiSSID);
  _prefs.putString("pass", data.wifiPassword);
  _prefs.putString("tz", data.timezone);
  _prefs.putString("city", data.city);
  _prefs.putString("owmkey", data.owmApiKey);
  _prefs.putBool("metric", data.metricUnits);
  _prefs.putBool("muted", data.muted);
  _prefs.end();
}

void Config::reset()
{
  _prefs.begin("device", false);
  _prefs.clear();
  _prefs.end();
}