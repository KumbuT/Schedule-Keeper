#include "Config.h"

Config& Config::instance() {
  static Config inst;
  return inst;
}

void Config::load() {
  _prefs.begin("device", true);
  data.wifiSSID     = _prefs.getString("ssid",    "");
  data.wifiPassword = _prefs.getString("pass",    "");
  data.timezone     = _prefs.getString("tz",      "UTC0");
  data.city         = _prefs.getString("city",    "London");
  data.owmApiKey    = _prefs.getString("owmkey",  "");
  data.metricUnits  = _prefs.getBool  ("metric",  true);
  data.muted        = _prefs.getBool  ("muted",   false);
  data.timerStyle   = _prefs.getInt   ("tstyle",  0);
  data.rewardStars  = _prefs.getInt   ("rstars",  0);
  data.rewardDay    = _prefs.getInt   ("rday",    0);
  data.streak       = _prefs.getInt   ("streak",  0);
  data.lastGoalDay  = _prefs.getInt   ("rgoalday",0);
  data.dailyGoal    = _prefs.getInt   ("rgoal",   5);
  if (data.dailyGoal < 1) data.dailyGoal = 1;
  _prefs.end();
}

void Config::save() {
  _prefs.begin("device", false);
  _prefs.putString("ssid",   data.wifiSSID);
  _prefs.putString("pass",   data.wifiPassword);
  _prefs.putString("tz",     data.timezone);
  _prefs.putString("city",   data.city);
  _prefs.putString("owmkey", data.owmApiKey);
  _prefs.putBool  ("metric", data.metricUnits);
  _prefs.putBool  ("muted",  data.muted);
  _prefs.putInt   ("tstyle", data.timerStyle);
  _prefs.putInt   ("rstars", data.rewardStars);
  _prefs.putInt   ("rday",   data.rewardDay);
  _prefs.putInt   ("streak", data.streak);
  _prefs.putInt   ("rgoalday", data.lastGoalDay);
  _prefs.putInt   ("rgoal",  data.dailyGoal);
  _prefs.end();
}

// Day-rollover + streak-break housekeeping. Safe to call often (~1/s): it only
// writes to NVS when something actually changes (i.e. at a day boundary).
void Config::rewardTick(int today) {
  if (today < 0) return; // clock not synced yet -- don't touch reward state
  bool changed = false;
  if (today != data.rewardDay) {           // new day -> reset today's stars
    data.rewardDay   = today;
    data.rewardStars = 0;
    changed = true;
  }
  // Streak breaks if the last goal-met day is older than yesterday.
  if (data.lastGoalDay > 0 && (today - data.lastGoalDay) > 1 && data.streak != 0) {
    data.streak = 0;
    changed = true;
  }
  if (changed) save();
}

// Record one completed task (a star). Increments the streak the moment the
// daily goal is first reached on a given day.
void Config::rewardStar(int today) {
  if (today < 0) return; // clock not synced yet
  rewardTick(today);
  data.rewardStars++;
  if (data.rewardStars == data.dailyGoal) {   // goal just reached today
    if (data.lastGoalDay == today - 1)      data.streak++;   // consecutive day
    else if (data.lastGoalDay != today)     data.streak = 1; // fresh streak
    data.lastGoalDay = today;
  }
  save();
}

// Parent correction: take one star back off today's count. Deliberately does
// NOT unwind an already-credited streak day (lenient toward the child, and
// avoids needing day history); it just lowers today's visible tally.
void Config::rewardRemove(int today) {
  if (today < 0) return;
  rewardTick(today);
  if (data.rewardStars > 0) data.rewardStars--;
  save();
}

void Config::reset() {
  _prefs.begin("device", false);
  _prefs.clear();
  _prefs.end();
}
