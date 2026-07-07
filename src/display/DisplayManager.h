#pragma once
#include <TFT_eSPI.h>
#include "../tasks/TaskScheduler.h"

struct WeatherData {
  float  temp       = 0;
  float  feelsLike  = 0;
  int    humidity   = 0;
  float  windSpeed  = 0;
  String description;
  String icon;
  bool   valid = false;
};

enum class Screen { HOME, TASK_LIST };

class DisplayManager {
public:
  static DisplayManager& instance();
  void begin();
  void update(struct tm* now);
  void setScreen(Screen s);

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute
  int pollTouch();

private:
  DisplayManager() {}

  TFT_eSPI    _tft;
  Screen      _screen = Screen::HOME;
  bool        _screenDirty = true;
  int         _scrollOffset = 0;    // For task list scrolling
  bool        _groupExpanded[16] = {};

  // Drawing methods
  void _drawStatusBar(struct tm* now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();

  // Widget helpers
  void _drawWifiArcs   (int cx, int cy, int rssi);
  void _drawBatteryIcon(int x,  int y,  int pct);
  void _drawProgressBar(int x,  int y,  int w, int h, float pct, uint32_t color);

  // Color palette (RGB565)
  static constexpr uint32_t CLR_BG       = 0x1082;
  static constexpr uint32_t CLR_CARD     = 0x2104;
  static constexpr uint32_t CLR_ACCENT   = 0x07FF;  // cyan
  static constexpr uint32_t CLR_TEXT     = 0xFFFF;
  static constexpr uint32_t CLR_SUBTEXT  = 0xAD75;  // gray
  static constexpr uint32_t CLR_GREEN    = 0x07E0;
  static constexpr uint32_t CLR_YELLOW   = 0xFFE0;
  static constexpr uint32_t CLR_RED      = 0xF800;
  static constexpr uint32_t CLR_BAR_BG   = 0x39C7;
  static constexpr uint32_t CLR_STATUSBG = 0x0841;
};