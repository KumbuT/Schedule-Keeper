#pragma once
#include <TFT_eSPI.h>
#include <vector>
#include "../tasks/TaskScheduler.h"

struct WeatherData
{
  float temp = 0;
  float feelsLike = 0;
  int humidity = 0;
  float windSpeed = 0;
  String description;
  String icon;
  String errorMsg; // Set on fetch failure: "API key invalid", "City not found", etc.
  bool valid = false;
};

enum class Screen
{
  HOME,
  TASK_LIST,
  TIMER_SET,
  TIMER_RUNNING
};

class DisplayManager
{
public:
  static DisplayManager &instance();
  void updateTaskListScroll(); // Call every loop() iteration; no-op unless on TASK_LIST
  void begin();
  void update(struct tm *now);
  void setScreen(Screen s);
  // void showClothingOverlay(); // Blocks until tap or 8s timeout, then redraws
  void showClothingOverlay(); // Now: draws and arms the overlay, returns immediately
  void tickOverlay();         // Call every loop() iteration while overlay is active
  bool overlayActive() const { return _overlayActive; }
  bool consumeDirty()
  {
    bool d = _screenDirty;
    _screenDirty = false;
    return d;
  }

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute, 3=back, 4=weather
  int pollTouch();
  void startTimer(uint32_t seconds);

private:
  int _touchStartY = 0;
  bool _isDragging = false;
  bool _overlayActive = false;
  uint32_t _overlayStart = 0;
  DisplayManager() {}
  TFT_eSPI _tft;
  Screen _screen = Screen::HOME;
  bool _screenDirty = true;
  int _scrollOffset = 0;   // For task list scrolling
  int _contentHeight = 0;  // total height of task list content (set by _drawTaskList)
  bool _touchDown = false; // true while finger is held down on TASK_LIST screen
  int _lastTouchY = 0;     // previous frame's touch Y, for delta calculation
  bool _groupExpanded[16] = {};
  float _gaugeAnimT = 0.0f; // Seconds elapsed — drives gauge bob and arc pulse

  // Drawing methods
  void _drawStatusBar(struct tm *now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();
  void _drawClothingOverlay(const std::vector<struct ClothingItem> &items);
  void _handleTaskListTap(int ty); // <-- add this line

  // Dial timer — 270° arc ring with emoji inside
  // cx=120, cy=174, r=64 for the 240×320 layout
  void _drawDial(int cx, int cy, int r,
                 float progressPct, int remainingSec, int totalSec,
                 float urgency, uint32_t arcColor);

  // Widget helpers
  void _drawWifiArcs(int cx, int cy, int rssi);
  void _drawBatteryIcon(int x, int y, int pct);
  void _drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color);
  void _drawTimerSet();
  void _drawTimerRunning();
  int _handleTimerSetTouch(uint16_t tx, uint16_t ty);
  int _handleTimerRunningTouch(uint16_t tx, uint16_t ty);

  uint32_t _timerDurationSec = 0;
  uint32_t _timerStartMillis = 0;
  bool _timerRunning = false;
  bool _timerDone = false;

  // Color palette (RGB565)
  static constexpr uint32_t CLR_BG = 0x1082;
  static constexpr uint32_t CLR_CARD = 0x2104;
  static constexpr uint32_t CLR_ACCENT = 0x07FF; // cyan
  static constexpr uint32_t CLR_TEXT = 0xFFFF;
  static constexpr uint32_t CLR_SUBTEXT = 0xAD75; // gray
  static constexpr uint32_t CLR_GREEN = 0x07E0;
  static constexpr uint32_t CLR_YELLOW = 0xFFE0;
  static constexpr uint32_t CLR_RED = 0xF800;
  static constexpr uint32_t CLR_BAR_BG = 0x39C7;
  static constexpr uint32_t CLR_STATUSBG = 0x0841;
  static constexpr uint32_t CLR_GAUGE_BG = 0x2945; // dial arc track
};
