#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>

struct Task {
  String id;
  String name;
  int    startHour;
  int    startMin;
  int    durationMin;
  String audioStart;
  String audioMid;
  String audioDone;
  String recurrence;    // "daily" | "weekdays" | "weekends"
  bool   enabled;
  bool   completedToday;
};

struct TaskGroup {
  String id;
  String name;
  String icon;
  uint32_t color;       // RGB888 stored, convert to RGB565 when drawing
  std::vector<Task> tasks;
};

enum class TaskEvent { START, MIDPOINT, ONE_MINUTE, COMPLETE };

class TaskScheduler {
public:
  static TaskScheduler& instance();

  bool loadFromFile(const char* path = "/schedule.json");
  bool saveToFile  (const char* path = "/schedule.json");

  void tick(struct tm* now);    // Call every second from main loop

  const Task*      currentTask()  const { return _currentTask; }
  const Task*      nextTask()     const { return _nextTask; }
  const TaskGroup* currentGroup() const { return _currentGroup; }
  int   elapsedSec()   const { return _elapsedSec; }
  int   remainingSec() const { return _remainingSec; }
  float progressPct()  const;

  std::vector<TaskGroup> groups;

  using EventCallback = std::function<void(const Task&, TaskEvent)>;
  void onEvent(EventCallback cb) { _eventCb = cb; }

private:
  TaskScheduler() {}
  void _findActiveTask(struct tm* now);
  bool _taskActiveNow (const Task& t, struct tm* now) const;
  bool _taskDueToday  (const Task& t, struct tm* now) const;

  const Task*      _currentTask  = nullptr;
  const Task*      _nextTask     = nullptr;
  const TaskGroup* _currentGroup = nullptr;
  int  _elapsedSec   = 0;
  int  _remainingSec = 0;
  bool _firedStart   = false;
  bool _firedMid     = false;
  bool _firedOneMin  = false;
  // No _firedDone -- COMPLETE now fires exactly once per active->inactive
  // (or active->next-task) transition, off a pointer comparison in tick(),
  // rather than a flag gated on _remainingSec <= 0 (see tick()'s comment
  // for why that could never actually trigger).

  EventCallback _eventCb;
};
