#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct DeviceConfig {
  String wifiSSID;
  String wifiPassword;
  String timezone;      // POSIX tz string e.g. "AEST-10AEDT,M10.1.0,M4.1.0/3"
  String city;          // City name for OpenWeatherMap
  String owmApiKey;     // OpenWeatherMap API key
  bool   metricUnits;   // true = Celsius/km, false = Fahrenheit/mph
  bool   muted;         // Audio mute state
};

class Config {
public:
  static Config& instance();
  void load();
  void save();
  void reset();

  DeviceConfig data;

private:
  Config() {}
  Preferences _prefs;
};
