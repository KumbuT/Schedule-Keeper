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
  TASK_LIST
};

class DisplayManager
{
public:
  static DisplayManager &instance();
  void begin();
  void update(struct tm *now);
  void setScreen(Screen s);
  void showClothingOverlay(); // Blocks until tap or 8s timeout, then redraws

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute, 3=back, 4=weather
  int pollTouch();

private:
  DisplayManager() {}

  TFT_eSPI _tft;
  Screen _screen = Screen::HOME;
  bool _screenDirty = true;
  int _scrollOffset = 0; // For task list scrolling
  bool _groupExpanded[16] = {};
  float _gaugeAnimT = 0.0f; // Seconds elapsed — drives gauge animation
  bool _blinkOn = true;     // Eye blink state
  float _blinkTimer = 3.0f; // Seconds until next blink toggle

  // Drawing methods
  void _drawStatusBar(struct tm *now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();
  void _drawClothingOverlay(const std::vector<struct ClothingItem> &items);

  // Bunny timeline animation — drawn above the progress bar track
  // barX/barY/barW: progress bar geometry
  // progressPct: 0–100
  // remainingSec/totalSec: for speed scaling
  void _drawBunny(int barX, int barY, int barW,
                  float progressPct, int remainingSec, int totalSec);

  // Gauge timer — replaces the flat progress bar with a circular arc gauge
  // containing the kawaii bunny in the centre.
  // cx/cy: centre of gauge on screen, r: outer radius
  void _drawGauge(int cx, int cy, int r, float progressPct,
                  int remainingSec, int totalSec);

  // Kawaii bunny face — drawn inside the gauge centre
  // urgency: 0.0=relaxed, 1.0=panicking
  void _drawKawaiBunny(int cx, int cy, float urgency);

  // Widget helpers
  void _drawWifiArcs(int cx, int cy, int rssi);
  void _drawBatteryIcon(int x, int y, int pct);
  void _drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color);

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

  // Kawaii bunny palette (RGB565)
  // Cream: #F5F0DC → R=0x1E, G=0x3C, B=0x1B → 0xF79B
  static constexpr uint32_t CLR_BUNNY_CREAM = 0xF79B;
  // Pink ear/cheek: #F0B8B0 → 0xF596
  static constexpr uint32_t CLR_BUNNY_PINK = 0xF596;
  // Outline: #1a1a1a → near black → 0x18C3
  static constexpr uint32_t CLR_BUNNY_OUTLINE = 0x18C3;
  // Gauge track background
  static constexpr uint32_t CLR_GAUGE_BG = 0x2945;
};