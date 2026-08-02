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
  int    timerStyle;    // running-timer look: 0 = digital clock, 1 = rainbow dial

  // ── Reward / streak state (persisted) ──────────────────────────────────────
  int    rewardStars;   // stars earned so far on rewardDay
  int    rewardDay;      // local day-index (days since 1970) the stars belong to
  int    streak;         // consecutive days the daily goal was met
  int    lastGoalDay;    // local day-index the goal was last met (0 = never)
  int    dailyGoal;      // stars needed in a day to keep the streak going
};

class Config {
public:
  static Config& instance();
  void load();
  void save();
  void reset();

  // Reward/streak helpers. `today` is a local day-index (days since 1970).
  // rewardTick() does day-rollover + streak-break housekeeping (call ~1/s);
  // rewardStar() records one completed task and updates the streak.
  void rewardTick(int today);
  void rewardStar(int today);   // parent grants a star
  void rewardRemove(int today); // parent takes a star back (correction)

  DeviceConfig data;

private:
  Config() {}
  Preferences _prefs;
};
