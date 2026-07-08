#include "TaskScheduler.h"
#include <ctime>
using std::difftime;
using std::gmtime;
using std::localtime;
using std::mktime;
using std::strftime;
using std::time;
using std::time_t;
using std::tm;
#include <LittleFS.h>

TaskScheduler &TaskScheduler::instance()
{
  static TaskScheduler inst;
  return inst;
}

bool TaskScheduler::loadFromFile(const char *path)
{
  File f = LittleFS.open(path, "r");
  if (!f)
  {
    Serial.println("[TaskScheduler] schedule.json not found");
    return false;
  }

  JsonDocument doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err != DeserializationError::Ok)
  {
    Serial.println("[TaskScheduler] JSON parse error");
    return false;
  }

  groups.clear();
  for (auto gv : doc["groups"].as<JsonArray>())
  {
    auto g = gv;
    TaskGroup grp;
    grp.id = g["id"].as<String>();
    grp.name = g["name"].as<String>();
    grp.icon = g["icon"].as<String>();
    // Parse hex color string "#RRGGBB"
    String colorStr = g["color"].as<String>();
    if (colorStr.isEmpty())
      colorStr = "#00BFFF";
    grp.color = strtoul(colorStr.substring(1).c_str(), nullptr, 16);

    for (auto tv : g["tasks"].as<JsonArray>())
    {
      auto t = tv;
      Task task;
      task.id = t["id"].as<String>();
      task.name = t["name"].as<String>();
      String st = t["start_time"].as<String>(); // "HH:MM"
      task.startHour = st.substring(0, 2).toInt();
      task.startMin = st.substring(3, 5).toInt();
      task.durationMin = t["duration_min"].as<int>();
      task.audioStart = t["audio_start"].as<String>();
      task.audioMid = t["audio_mid"].as<String>();
      task.audioDone = t["audio_done"].as<String>();
      task.recurrence = t["recurrence"].as<String>();
      if (task.recurrence.isEmpty())
        task.recurrence = "daily";
      task.enabled = t["enabled"].as<bool>() ? t["enabled"].as<bool>() : true;
      task.completedToday = false;
      grp.tasks.push_back(task);
    }
    groups.push_back(grp);
  }

  Serial.printf("[TaskScheduler] Loaded %d groups\n", groups.size());
  return true;
}

bool TaskScheduler::_taskDueToday(const Task &t, std::tm *now) const
{
  if (t.recurrence == "daily")
    return true;
  int dow = now->tm_wday; // 0 = Sunday
  if (t.recurrence == "weekdays")
    return dow >= 1 && dow <= 5;
  if (t.recurrence == "weekends")
    return dow == 0 || dow == 6;
  return true;
}

bool TaskScheduler::_taskActiveNow(const Task &t, std::tm *now) const
{
  if (!t.enabled || !_taskDueToday(t, now))
    return false;
  int nowMin = now->tm_hour * 60 + now->tm_min;
  int startMin = t.startHour * 60 + t.startMin;
  int endMin = startMin + t.durationMin;
  return nowMin >= startMin && nowMin < endMin;
}

void TaskScheduler::_findActiveTask(std::tm *now)
{
  int nowMin = now->tm_hour * 60 + now->tm_min;
  _currentTask = nullptr;
  _currentGroup = nullptr;
  _nextTask = nullptr;

  int nextDelta = INT_MAX;

  for (auto &grp : groups)
  {
    for (auto &task : grp.tasks)
    {
      if (!task.enabled || !_taskDueToday(const_cast<Task &>(task), now))
        continue;

      if (_taskActiveNow(task, now))
      {
        _currentTask = &task;
        _currentGroup = &grp;
      }
      else
      {
        int startMin = task.startHour * 60 + task.startMin;
        int delta = startMin - nowMin;
        if (delta > 0 && delta < nextDelta)
        {
          nextDelta = delta;
          _nextTask = &task;
        }
      }
    }
  }
}

void TaskScheduler::tick(std::tm *now)
{
  bool wasActive = (_currentTask != nullptr);
  _findActiveTask(now);

  if (!_currentTask)
  {
    _elapsedSec = _remainingSec = 0;
    if (wasActive)
    {
      // Task just ended, reset fire flags for next task
      _firedStart = _firedMid = _firedOneMin = _firedDone = false;
    }
    return;
  }

  int nowSec = now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec;
  int startSec = _currentTask->startHour * 3600 + _currentTask->startMin * 60;
  int durSec = _currentTask->durationMin * 60;

  _elapsedSec = nowSec - startSec;
  _remainingSec = durSec - _elapsedSec;

  // Reset fire flags when a new task becomes active
  static String lastTaskId = "";
  if (_currentTask->id != lastTaskId)
  {
    lastTaskId = _currentTask->id;
    _firedStart = false;
    _firedMid = false;
    _firedOneMin = false;
    _firedDone = false;
  }

  if (!_firedStart && _elapsedSec >= 0)
  {
    if (_eventCb)
      _eventCb(*_currentTask, TaskEvent::START);
    _firedStart = true;
  }
  if (!_firedMid && _elapsedSec >= durSec / 2)
  {
    if (_eventCb)
      _eventCb(*_currentTask, TaskEvent::MIDPOINT);
    _firedMid = true;
  }
  if (!_firedOneMin && _remainingSec <= 60 && _remainingSec > 0)
  {
    if (_eventCb)
      _eventCb(*_currentTask, TaskEvent::ONE_MINUTE);
    _firedOneMin = true;
  }
  if (!_firedDone && _remainingSec <= 0)
  {
    if (_eventCb)
      _eventCb(*_currentTask, TaskEvent::COMPLETE);
    _firedDone = true;
  }
}

float TaskScheduler::progressPct() const
{
  if (!_currentTask)
    return 0.0f;
  int dur = _currentTask->durationMin * 60;
  return dur > 0 ? (float)_elapsedSec / (float)dur * 100.0f : 0.0f;
}
