#include "DisplayManager.h"
#include <WiFi.h>
#include "../config/Config.h"
#include "../sensors/BatteryMonitor.h"

DisplayManager& DisplayManager::instance() {
  static DisplayManager inst;
  return inst;
}

void DisplayManager::begin() {
  _tft.init();
  _tft.setRotation(0);  // Portrait
  _tft.fillScreen(CLR_BG);
  _tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < 16; i++) _groupExpanded[i] = true;  // All expanded by default
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi Arc Renderer
// Draws 4 quarter-circle arcs (bottom arc style, like Android/iOS)
// cx, cy = anchor point (base center of arc stack)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWifiArcs(int cx, int cy, int rssi) {
  int bars = 0;
  if      (rssi == 0)   bars = 0;  // disconnected
  else if (rssi >= -55) bars = 4;
  else if (rssi >= -67) bars = 3;
  else if (rssi >= -78) bars = 2;
  else                  bars = 1;

  if (rssi == 0) {
    _tft.drawLine(cx-4, cy-4, cx+4, cy+4, CLR_RED);
    _tft.drawLine(cx+4, cy-4, cx-4, cy+4, CLR_RED);
    return;
  }

  // 4 arcs: increasing radius from center-bottom anchor
  const int radii[4]    = { 4, 7, 10, 13 };
  const int thickness   = 2;

  for (int i = 0; i < 4; i++) {
    uint32_t color = (i < bars) ? CLR_ACCENT : CLR_SUBTEXT;
    // drawSmoothArc(cx, cy, r, ir, startAngle, endAngle, fg, bg)
    // Angles: 225°→315° gives a bottom-upward arc sweep
    _tft.drawSmoothArc(cx, cy + radii[3],
                       radii[i],
                       radii[i] - thickness,
                       215, 325,
                       color, CLR_STATUSBG);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Battery Icon
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawBatteryIcon(int x, int y, int pct) {
  uint32_t fillColor = pct > 50 ? CLR_GREEN : (pct > 20 ? CLR_YELLOW : CLR_RED);

  _tft.drawRect(x, y, 22, 10, CLR_TEXT);
  _tft.fillRect(x + 22, y + 3, 2, 4, CLR_TEXT);  // nub

  int fillW = max(1, (int)(pct / 100.0f * 20));
  _tft.fillRect(x + 1, y + 1, fillW, 8, fillColor);

  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.setTextSize(1);
  _tft.setCursor(x + 26, y + 1);
  _tft.printf("%d%%", pct);
}

// ─────────────────────────────────────────────────────────────────────────────
// Progress Bar
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color) {
  _tft.fillRect(x, y, w, h, CLR_BAR_BG);
  int filled = (int)(pct / 100.0f * w);
  if (filled > 0) _tft.fillRect(x, y, filled, h, color);
  // Rounded end cap
  if (filled > 0 && filled < w)
    _tft.fillCircle(x + filled, y + h/2, h/2, color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status Bar (top 20px)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawStatusBar(struct tm* now) {
  _tft.fillRect(0, 0, 240, 20, CLR_STATUSBG);

  // WiFi arcs — left side
  int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
  _drawWifiArcs(14, 6, rssi);

  // Battery — right side
  _drawBatteryIcon(170, 5, BatteryMonitor::instance().percentage());

  // Time — center
  char timeBuf[6];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M", now);
  _tft.setTextDatum(TC_DATUM);
  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.setTextSize(1);
  _tft.drawString(timeBuf, 120, 4);

  // Date — small, below time
  char dateBuf[12];
  strftime(dateBuf, sizeof(dateBuf), "%a %d %b", now);
  _tft.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _tft.drawString(dateBuf, 120, 12);

  _tft.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Weather Row (y=20 to y=58)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWeatherRow() {
  _tft.fillRect(0, 20, 240, 38, CLR_CARD);

  if (!weather.valid) {
    _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _tft.setTextSize(1);
    _tft.setCursor(8, 32);
    _tft.print("Weather unavailable");
    return;
  }

  auto& cfg = Config::instance();
  const char* unit = cfg.data.metricUnits ? "C" : "F";

  _tft.setTextColor(CLR_TEXT, CLR_CARD);
  _tft.setTextSize(2);
  _tft.setCursor(8, 24);
  _tft.printf("%.0f\xF7%s", weather.temp, unit);   // degree symbol = 0xF7 in default font

  _tft.setTextSize(1);
  _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
  _tft.setCursor(70, 24);
  _tft.print(weather.description.substring(0, 20).c_str());

  _tft.setCursor(70, 36);
  _tft.printf("H:%d%%  W:%.0f%s",
              weather.humidity,
              weather.windSpeed,
              cfg.data.metricUnits ? "km/h" : "mph");
}

// ─────────────────────────────────────────────────────────────────────────────
// Current Task Card (y=60 to y=258)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawCurrentTask() {
  _tft.fillRect(0, 60, 240, 198, CLR_BG);

  auto& sched = TaskScheduler::instance();
  const Task* t = sched.currentTask();

  if (!t) {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);
    _tft.drawString("No active task", 120, 155);
    _tft.setTextDatum(TL_DATUM);
    return;
  }

  // Group label
  if (sched.currentGroup()) {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextSize(1);
    _tft.setCursor(10, 68);
    _tft.print(sched.currentGroup()->name.c_str());
  }

  // Task name (large)
  _tft.setTextColor(CLR_TEXT, CLR_BG);
  _tft.setTextSize(2);
  _tft.setCursor(10, 82);
  _tft.print(t->name.substring(0, 16).c_str());

  // Time remaining (large, right-aligned)
  int rem  = sched.remainingSec();
  int remM = rem / 60;
  int remS = rem % 60;
  char remBuf[8];
  snprintf(remBuf, sizeof(remBuf), "%d:%02d", remM, remS);
  _tft.setTextDatum(TR_DATUM);
  _tft.setTextColor(CLR_ACCENT, CLR_BG);
  _tft.setTextSize(2);
  _tft.drawString(remBuf, 230, 82);
  _tft.setTextDatum(TL_DATUM);

  // Progress bar
  _drawProgressBar(10, 112, 220, 10, sched.progressPct(), CLR_ACCENT);

  // Progress percentage label
  _tft.setTextSize(1);
  _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
  _tft.setCursor(10, 128);
  _tft.printf("%.0f%% complete", sched.progressPct());

  // Duration info
  _tft.setCursor(10, 142);
  _tft.printf("Duration: %d min", t->durationMin);

  // Next task
  const Task* next = sched.nextTask();
  if (next) {
    _tft.fillRect(0, 220, 240, 40, CLR_CARD);
    _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _tft.setTextSize(1);
    _tft.setCursor(10, 228);
    _tft.print("NEXT UP");
    _tft.setTextColor(CLR_TEXT, CLR_CARD);
    _tft.setCursor(10, 240);
    _tft.print(next->name.substring(0, 24).c_str());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation Bar (y=260 to y=320)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawNavBar() {
  int y = 262;
  _tft.drawFastHLine(0, y, 240, CLR_SUBTEXT);
  _tft.fillRect(0,   y+1, 120, 58, CLR_CARD);
  _tft.fillRect(120, y+1, 120, 58, CLR_CARD);
  _tft.drawFastVLine(120, y, 58, CLR_SUBTEXT);

  _tft.setTextDatum(MC_DATUM);
  _tft.setTextSize(1);

  // All Tasks button
  _tft.setTextColor(CLR_TEXT, CLR_CARD);
  _tft.drawString("All Tasks", 60, y + 20);
  _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
  _tft.drawString("View schedule", 60, y + 34);

  // Mute button
  bool muted = Config::instance().data.muted;
  _tft.setTextColor(muted ? CLR_RED : CLR_TEXT, CLR_CARD);
  _tft.drawString(muted ? "Unmute" : "Mute", 180, y + 20);
  _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
  _tft.drawString(muted ? "Audio off" : "Audio on", 180, y + 34);

  _tft.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task List Screen
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawTaskList() {
  _tft.fillScreen(CLR_BG);

  // Header
  _tft.fillRect(0, 0, 240, 30, CLR_STATUSBG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.setTextSize(1);
  _tft.drawString("All Tasks", 120, 15);
  _tft.setTextDatum(TL_DATUM);
  _tft.setTextColor(CLR_ACCENT, CLR_STATUSBG);
  _tft.drawString("< Back", 8, 11);

  auto& sched = TaskScheduler::instance();
  int y = 36 - _scrollOffset;

  for (int gi = 0; gi < (int)sched.groups.size(); gi++) {
    auto& grp = sched.groups[gi];
    if (y > 320) break;

    // Group header row
    if (y > 20) {
      _tft.fillRect(0, y, 240, 24, CLR_CARD);
      // Color accent bar
      uint32_t c565 = ((grp.color & 0xF80000) >> 8) |
                      ((grp.color & 0x00FC00) >> 5) |
                      ((grp.color & 0x0000F8) >> 3);
      _tft.fillRect(0, y, 4, 24, c565);
      _tft.setTextColor(CLR_TEXT, CLR_CARD);
      _tft.setTextSize(1);
      _tft.setCursor(10, y + 8);
      _tft.print(grp.name.c_str());
      // Expand/collapse indicator
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.setCursor(218, y + 8);
      _tft.print(_groupExpanded[gi] ? "v" : ">");
    }
    y += 26;

    if (!_groupExpanded[gi]) continue;

    // Task rows
    for (auto& task : grp.tasks) {
      if (y > 320) break;
      if (y > 20) {
        bool isActive = (TaskScheduler::instance().currentTask() &&
                         TaskScheduler::instance().currentTask()->id == task.id);

        _tft.fillRect(0, y, 240, 22, isActive ? 0x0842 : CLR_BG);
        if (isActive) {
          _tft.fillRect(0, y, 3, 22, CLR_ACCENT);
        }

        // Status dot
        uint32_t dotColor = task.completedToday ? CLR_GREEN :
                            (isActive ? CLR_ACCENT : CLR_SUBTEXT);
        _tft.fillCircle(16, y + 11, 4, dotColor);

        // Task name
        _tft.setTextColor(isActive ? CLR_ACCENT : CLR_TEXT,
                          isActive ? 0x0842 : CLR_BG);
        _tft.setTextSize(1);
        _tft.setCursor(28, y + 7);
        _tft.print(task.name.substring(0, 18).c_str());

        // Start time + duration (right side)
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d %dm",
                 task.startHour, task.startMin, task.durationMin);
        _tft.setTextColor(CLR_SUBTEXT, isActive ? 0x0842 : CLR_BG);
        _tft.setTextDatum(TR_DATUM);
        _tft.drawString(tbuf, 234, y + 7);
        _tft.setTextDatum(TL_DATUM);
      }
      y += 24;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main update entry point
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::update(struct tm* now) {
  if (_screen == Screen::HOME) {
    _drawStatusBar(now);
    _drawWeatherRow();
    _drawCurrentTask();
    _drawNavBar();
  } else {
    _drawTaskList();
  }
}

void DisplayManager::setScreen(Screen s) {
  _screen = s;
  _tft.fillScreen(CLR_BG);
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch polling
// Returns: -1=no touch, 0=main area, 1=All Tasks, 2=Mute, 3=Back (task list)
// ─────────────────────────────────────────────────────────────────────────────
int DisplayManager::pollTouch() {
  uint16_t tx, ty;
  if (!_tft.getTouch(&tx, &ty)) return -1;

  if (_screen == Screen::TASK_LIST) {
    if (ty < 30) return 3;  // Back button
    return 0;
  }

  // HOME screen nav bar (y > 262)
  if (ty > 262) {
    return (tx < 120) ? 1 : 2;
  }

  return 0;
}