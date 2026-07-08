#include "DisplayManager.h"
#include <WiFi.h>
#include "../config/Config.h"
#include "../sensors/BatteryMonitor.h"

DisplayManager &DisplayManager::instance()
{
  static DisplayManager inst;
  return inst;
}

void DisplayManager::begin()
{
  _tft.init();
  _tft.setRotation(0); // Portrait
  _tft.fillScreen(CLR_BG);
  _tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < 16; i++)
    _groupExpanded[i] = true; // All expanded by default
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi Arc Renderer
// Draws 4 quarter-circle arcs (bottom arc style, like Android/iOS)
// cx, cy = anchor point (base center of arc stack)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWifiArcs(int cx, int cy, int rssi)
{
  int bars = 0;
  if (rssi == 0)
    bars = 0; // disconnected
  else if (rssi >= -55)
    bars = 4;
  else if (rssi >= -67)
    bars = 3;
  else if (rssi >= -78)
    bars = 2;
  else
    bars = 1;

  if (rssi == 0)
  {
    _tft.drawLine(cx - 4, cy - 4, cx + 4, cy + 4, CLR_RED);
    _tft.drawLine(cx + 4, cy - 4, cx - 4, cy + 4, CLR_RED);
    return;
  }

  // 4 arcs: increasing radius from center-bottom anchor
  const int radii[4] = {4, 7, 10, 13};
  const int thickness = 2;

  for (int i = 0; i < 4; i++)
  {
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
void DisplayManager::_drawBatteryIcon(int x, int y, int pct)
{
  uint32_t fillColor = pct > 50 ? CLR_GREEN : (pct > 20 ? CLR_YELLOW : CLR_RED);

  _tft.drawRect(x, y, 22, 10, CLR_TEXT);
  _tft.fillRect(x + 22, y + 3, 2, 4, CLR_TEXT); // nub

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
void DisplayManager::_drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color)
{
  _tft.fillRect(x, y, w, h, CLR_BAR_BG);
  int filled = (int)(pct / 100.0f * w);
  if (filled > 0)
    _tft.fillRect(x, y, filled, h, color);
  // Rounded end cap
  if (filled > 0 && filled < w)
    _tft.fillCircle(x + filled, y + h / 2, h / 2, color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status Bar (top 20px)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawStatusBar(struct tm *now)
{
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
// Shows error messages for specific failure modes (bad key, bad city, etc.)
// so the user knows to visit /config rather than just seeing a blank row.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWeatherRow()
{
  _tft.fillRect(0, 20, 240, 38, CLR_CARD);

  if (!weather.valid)
  {
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);

    if (weather.errorMsg == "API key invalid")
    {
      _tft.setTextColor(CLR_RED, CLR_CARD);
      _tft.drawString("Weather: invalid API key", 120, 28);
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Fix key at /config", 120, 40);
    }
    else if (weather.errorMsg == "City not found")
    {
      _tft.setTextColor(CLR_RED, CLR_CARD);
      _tft.drawString("Weather: city not found", 120, 28);
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Fix city at /config", 120, 40);
    }
    else if (!weather.errorMsg.isEmpty())
    {
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString(weather.errorMsg.c_str(), 120, 34);
    }
    else
    {
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Weather unavailable", 120, 34);
    }

    _tft.setTextDatum(TL_DATUM);
    return;
  }

  auto &cfg = Config::instance();
  const char *unit = cfg.data.metricUnits ? "C" : "F";

  _tft.setTextColor(CLR_TEXT, CLR_CARD);
  _tft.setTextSize(2);
  _tft.setCursor(8, 24);
  _tft.printf("%.0f\xF7%s", weather.temp, unit); // degree symbol = 0xF7 in default font

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
// Current Task Card (y=58 to y=262) — gauge layout
//
// Layout (portrait 240×320):
//   y=58–70:  Group label + task name (centred)
//   y=72–238: Circular gauge (87px radius, centred at x=120 y=155)
//             └─ Kawaii bunny face in centre
//             └─ Time remaining text below bunny inside arc
//   y=240–262: Next task card
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawCurrentTask()
{
  _tft.fillRect(0, 58, 240, 204, CLR_BG);

  auto &sched = TaskScheduler::instance();
  const Task *t = sched.currentTask();

  if (!t)
  {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);
    _tft.drawString("No active task", 120, 155);
    _tft.setTextDatum(TL_DATUM);
    return;
  }

  // ── Group label + task name (top, centred) ──────────────────────────────
  if (sched.currentGroup())
  {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(sched.currentGroup()->name.c_str(), 120, 62);
  }
  _tft.setTextColor(CLR_TEXT, CLR_BG);
  _tft.setTextSize(2);
  _tft.setTextDatum(TC_DATUM);
  // Truncate to fit — size 2 font is ~12px per char, 16 chars = 192px
  _tft.drawString(t->name.substring(0, 14).c_str(), 120, 74);
  _tft.setTextDatum(TL_DATUM);

  // ── Circular gauge (centred in remaining space) ──────────────────────────
  _drawGauge(120, 162, 74,
             sched.progressPct(),
             sched.remainingSec(),
             t->durationMin * 60);

  // ── Next task card ────────────────────────────────────────────────────────
  const Task *next = sched.nextTask();
  if (next)
  {
    _tft.fillRect(0, 242, 240, 20, CLR_CARD);
    _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setCursor(8, 248);
    _tft.print("NEXT: ");
    _tft.setTextColor(CLR_TEXT, CLR_CARD);
    _tft.print(next->name.substring(0, 20).c_str());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation Bar (y=260 to y=320)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawNavBar()
{
  int y = 262;
  _tft.drawFastHLine(0, y, 240, CLR_SUBTEXT);
  _tft.fillRect(0, y + 1, 120, 58, CLR_CARD);
  _tft.fillRect(120, y + 1, 120, 58, CLR_CARD);
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
void DisplayManager::_drawTaskList()
{
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

  auto &sched = TaskScheduler::instance();
  int y = 36 - _scrollOffset;

  for (int gi = 0; gi < (int)sched.groups.size(); gi++)
  {
    auto &grp = sched.groups[gi];
    if (y > 320)
      break;

    // Group header row
    if (y > 20)
    {
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

    if (!_groupExpanded[gi])
      continue;

    // Task rows
    for (auto &task : grp.tasks)
    {
      if (y > 320)
        break;
      if (y > 20)
      {
        bool isActive = (TaskScheduler::instance().currentTask() &&
                         TaskScheduler::instance().currentTask()->id == task.id);

        _tft.fillRect(0, y, 240, 22, isActive ? 0x0842 : CLR_BG);
        if (isActive)
        {
          _tft.fillRect(0, y, 3, 22, CLR_ACCENT);
        }

        // Status dot
        uint32_t dotColor = task.completedToday ? CLR_GREEN : (isActive ? CLR_ACCENT : CLR_SUBTEXT);
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
// Main update entry point — called every second from loop()
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::update(struct tm *now)
{
  _gaugeAnimT += 1.0f; // 1 second per update() call — drives all animation

  // ── Blink timer — rate varies by urgency state ───────────────────────────
  // Compute current urgency from the active task (if any) for blink rate
  auto &sched = TaskScheduler::instance();
  float blinkUrgency = 0.0f;
  if (sched.currentTask() && sched.currentTask()->durationMin > 0)
  {
    int totalSec = sched.currentTask()->durationMin * 60;
    blinkUrgency = (totalSec > 0)
                       ? 1.0f - ((float)sched.remainingSec() / (float)totalSec)
                       : 0.0f;
  }
  const bool blinkIsPanic = blinkUrgency >= 0.8f;
  const bool blinkIsWorry = blinkUrgency >= 0.5f && !blinkIsPanic;

  _blinkTimer -= 1.0f;
  if (_blinkTimer <= 0.0f)
  {
    _blinkOn = !_blinkOn;
    if (_blinkOn)
    {
      // Eyes open — duration shorter when anxious (rapid nervous blinking)
      float openDur = blinkIsPanic   ? 0.8f + (rand() % 70) / 100.0f   // 0.8–1.5s
                      : blinkIsWorry ? 2.0f + (rand() % 150) / 100.0f  // 2.0–3.5s
                                     : 3.0f + (rand() % 300) / 100.0f; // 3.0–6.0s
      _blinkTimer = openDur;
    }
    else
    {
      _blinkTimer = 0.12f; // closed duration same across all states
    }
  }

  if (_screen == Screen::HOME)
  {
    _drawStatusBar(now);
    _drawWeatherRow();
    _drawCurrentTask();
    _drawNavBar();
  }
  else
  {
    _drawTaskList();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Circular gauge timer
//
// Draws a 270° arc (from ~8 o'clock sweeping clockwise) showing time remaining.
// Arc colour transitions: cyan (>50%) → yellow (20–50%) → red (<20%, pulsing).
// The kawaii bunny is centred inside the arc. Time remaining is shown in digits
// below the bunny inside the gauge circle.
//
// cx, cy  — centre pixel on the 240×320 screen
// r       — outer radius of the arc ring (74px fits the layout)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawGauge(int cx, int cy, int r,
                                float progressPct,
                                int remainingSec, int totalSec)
{
  const float urgency = (totalSec > 0)
                            ? 1.0f - ((float)remainingSec / (float)totalSec)
                            : 1.0f;

  // Arc colour — matches v2 visualizer
  uint32_t arcColor;
  if (urgency > 0.8f)
  {
    // Pulse between red and darker red
    bool pulse = ((int)_gaugeAnimT % 2 == 0);
    arcColor = pulse ? CLR_RED : 0xA800;
  }
  else if (urgency > 0.5f)
  {
    arcColor = CLR_YELLOW;
  }
  else
  {
    arcColor = CLR_ACCENT;
  }

  const int STROKE = 10;
  const int r_arc = r - STROKE / 2;

  // ── Track (background ring) ──────────────────────────────────────────────
  // TFT_eSPI drawSmoothArc(cx, cy, r, ir, startAngle, endAngle, fg, bg)
  // Angles: 225°→495° = 270° sweep from bottom-left to bottom-right
  _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                     225, 495, CLR_GAUGE_BG, CLR_BG);

  // ── Remaining arc ────────────────────────────────────────────────────────
  if (urgency < 1.0f)
  {
    float endDeg = 225.0f + 270.0f * (1.0f - urgency);
    _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                       225, (int)endDeg, arcColor, CLR_BG);
  }

  // ── Kawaii bunny face in centre ───────────────────────────────────────────
  _drawKawaiBunny(cx, cy - 8, urgency);

  // ── Time remaining text (below bunny, inside arc) ─────────────────────────
  int remM = remainingSec / 60;
  int remS = remainingSec % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", remM, remS);

  _tft.setTextDatum(TC_DATUM);
  _tft.setTextSize(2);
  _tft.setTextColor(arcColor, CLR_BG);
  _tft.drawString(timeBuf, cx, cy + 40);

  _tft.setTextSize(1);
  _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
  _tft.drawString("remaining", cx, cy + 58);
  _tft.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Kawaii bunny face — rendered inside the gauge circle
//
// Style: reference image — cream colour (#F5F0DC), thick dark outline,
// chubby rounded head, tall ears with pink inner, dot eyes with rosy cheeks,
// ω mouth. Three states driven by urgency:
//
//   relax  (urgency < 0.5)  — upright ears, dot eyes with slow blink,
//                              ω smile, gentle bob, Zzz when very early
//   worry  (urgency 0.5–0.8)— ears swept back, angled worry lines, flat mouth
//   panic  (urgency > 0.8)  — ears flat, wide shock eyes, open frown,
//                              trembling, sweat drop indicators
//
// All drawn with TFT_eSPI filled circles, arcs, and lines — no bitmaps.
// Fits a 56×56px bounding box centred on (cx, cy).
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawKawaiBunny(int cx, int cy, float urgency)
{
  // Determine state
  const bool isRelax = urgency < 0.5f;
  const bool isWorry = urgency >= 0.5f && urgency < 0.8f;
  const bool isPanic = urgency >= 0.8f;

  // ── Bob animation — runs in all states, scaled by urgency ───────────────
  // Relax: slow lazy bob ±2.5px at 1.6 rad/s
  // Worry: smaller anxious fidget ±1px at 3.5 rad/s
  // Panic: trembling overrides bob (shake is more expressive than a bob)
  float bobAmt = isRelax ? 2.5f : isWorry ? 1.0f
                                          : 0.0f;
  float bobFreq = isRelax ? 1.6f : 3.5f;
  int bob = isPanic ? 0 : (int)(sinf(_gaugeAnimT * bobFreq) * bobAmt);

  // ── Panic trembling (replaces bob) ──────────────────────────────────────
  int shakeX = 0, shakeY = 0;
  if (isPanic)
  {
    shakeX = (int)(sinf(_gaugeAnimT * 38.0f) * 1.8f);
    shakeY = (int)(cosf(_gaugeAnimT * 35.0f) * 1.2f);
  }

  const int bx = cx + shakeX;
  const int by = cy + bob + shakeY;

  // ── Ears (drawn first, behind head) ────────────────────────────────────
  if (isRelax)
  {
    // Tall upright ears
    // Left ear: outer cream ellipse with outline, inner pink
    _tft.fillEllipse(bx - 10, by - 30, 7, 14, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx - 10, by - 30, 7, 14, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx - 10, by - 30, 4, 9, CLR_BUNNY_PINK);
    // Right ear
    _tft.fillEllipse(bx + 10, by - 30, 7, 14, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx + 10, by - 30, 7, 14, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx + 10, by - 30, 4, 9, CLR_BUNNY_PINK);
  }
  else if (isWorry)
  {
    // Ears swept back — angled ellipses
    _tft.fillEllipse(bx - 14, by - 26, 6, 13, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx - 14, by - 26, 6, 13, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx - 14, by - 26, 3, 8, CLR_BUNNY_PINK);
    _tft.fillEllipse(bx + 14, by - 26, 6, 13, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx + 14, by - 26, 6, 13, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx + 14, by - 26, 3, 8, CLR_BUNNY_PINK);
  }
  else
  {
    // Ears flat to sides
    _tft.fillEllipse(bx - 18, by - 18, 8, 6, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx - 18, by - 18, 8, 6, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx - 18, by - 18, 5, 3, CLR_BUNNY_PINK);
    _tft.fillEllipse(bx + 18, by - 18, 8, 6, CLR_BUNNY_CREAM);
    _tft.drawEllipse(bx + 18, by - 18, 8, 6, CLR_BUNNY_OUTLINE);
    _tft.fillEllipse(bx + 18, by - 18, 5, 3, CLR_BUNNY_PINK);
  }

  // ── Head — chubby wider-than-tall ellipse ─────────────────────────────
  _tft.fillEllipse(bx, by, 24, 22, CLR_BUNNY_CREAM);
  _tft.drawEllipse(bx, by, 24, 22, CLR_BUNNY_OUTLINE);

  // ── Rosy cheeks ───────────────────────────────────────────────────────
  _tft.fillEllipse(bx - 14, by + 5, 6, 4, CLR_BUNNY_PINK);
  _tft.fillEllipse(bx + 14, by + 5, 6, 4, CLR_BUNNY_PINK);

  // ── Eyes ──────────────────────────────────────────────────────────────
  const int eyeY = by - 3;
  const int eyeX = 8; // offset from centre

  if (isRelax)
  {
    if (_blinkOn)
    {
      // Dot eyes open
      _tft.fillCircle(bx - eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.fillCircle(bx + eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.drawPixel(bx - eyeX + 1, eyeY - 1, CLR_TEXT);
      _tft.drawPixel(bx + eyeX + 1, eyeY - 1, CLR_TEXT);
    }
    else
    {
      // Relaxed blink — gentle ^ arc
      _tft.drawArc(bx - eyeX, eyeY + 1, 3, 1, 200, 340, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
      _tft.drawArc(bx + eyeX, eyeY + 1, 3, 1, 200, 340, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
    }
  }
  else if (isWorry)
  {
    if (_blinkOn)
    {
      // Slightly larger dot eyes open + worry lines
      _tft.fillCircle(bx - eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.fillCircle(bx + eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.drawPixel(bx - eyeX + 1, eyeY - 1, CLR_TEXT);
      _tft.drawPixel(bx + eyeX + 1, eyeY - 1, CLR_TEXT);
    }
    else
    {
      // Worried squint — tight horizontal line rather than full close
      _tft.drawLine(bx - eyeX - 3, eyeY, bx - eyeX + 3, eyeY + 1, CLR_BUNNY_OUTLINE);
      _tft.drawLine(bx + eyeX - 3, eyeY, bx + eyeX + 3, eyeY + 1, CLR_BUNNY_OUTLINE);
    }
    // Worry mark lines always visible regardless of blink state
    _tft.drawLine(bx - 13, eyeY - 7, bx - 7, eyeY - 5, CLR_SUBTEXT);
    _tft.drawLine(bx + 13, eyeY - 7, bx + 7, eyeY - 5, CLR_SUBTEXT);
  }
  else
  {
    // Panic
    if (_blinkOn)
    {
      // Wide shock eyes open
      _tft.fillCircle(bx - eyeX, eyeY, 5, CLR_TEXT);
      _tft.drawCircle(bx - eyeX, eyeY, 5, CLR_BUNNY_OUTLINE);
      _tft.fillCircle(bx - eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.drawPixel(bx - eyeX + 1, eyeY - 1, CLR_TEXT);
      _tft.fillCircle(bx + eyeX, eyeY, 5, CLR_TEXT);
      _tft.drawCircle(bx + eyeX, eyeY, 5, CLR_BUNNY_OUTLINE);
      _tft.fillCircle(bx + eyeX, eyeY, 3, CLR_BUNNY_OUTLINE);
      _tft.drawPixel(bx + eyeX + 1, eyeY - 1, CLR_TEXT);
    }
    else
    {
      // Panic scrunch — both eyes shut tight, single dark line
      _tft.drawLine(bx - eyeX - 3, eyeY - 1, bx - eyeX + 3, eyeY + 1, CLR_BUNNY_OUTLINE);
      _tft.drawLine(bx + eyeX - 3, eyeY - 1, bx + eyeX + 3, eyeY + 1, CLR_BUNNY_OUTLINE);
    }
    // Stress lines always visible regardless of blink state
    _tft.drawLine(bx - eyeX - 4, eyeY - 4, bx - eyeX - 7, eyeY - 7, CLR_SUBTEXT);
    _tft.drawLine(bx - eyeX - 5, eyeY, bx - eyeX - 9, eyeY, CLR_SUBTEXT);
    _tft.drawLine(bx + eyeX + 4, eyeY - 4, bx + eyeX + 7, eyeY - 7, CLR_SUBTEXT);
    _tft.drawLine(bx + eyeX + 5, eyeY, bx + eyeX + 9, eyeY, CLR_SUBTEXT);
  }

  // ── Mouth ────────────────────────────────────────────────────────────
  const int mouthY = by + 10;
  if (isRelax)
  {
    // ω shape — two small quadratic bumps
    // Approximated with two small arcs
    _tft.drawArc(bx - 4, mouthY + 1, 3, 1, 0, 180, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
    _tft.drawArc(bx + 4, mouthY + 1, 3, 1, 0, 180, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
  }
  else if (isWorry)
  {
    // Flat w — bumps less pronounced
    _tft.drawArc(bx - 4, mouthY + 2, 3, 1, 0, 180, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
    _tft.drawArc(bx + 4, mouthY + 2, 3, 1, 0, 180, CLR_BUNNY_OUTLINE, CLR_BUNNY_CREAM);
    // Wavy uncertainty line below
    _tft.drawLine(bx - 5, mouthY + 7, bx - 1, mouthY + 5, CLR_SUBTEXT);
    _tft.drawLine(bx - 1, mouthY + 5, bx + 3, mouthY + 7, CLR_SUBTEXT);
    _tft.drawLine(bx + 3, mouthY + 7, bx + 6, mouthY + 6, CLR_SUBTEXT);
  }
  else
  {
    // Open frown — two lines meeting at a dip
    _tft.drawLine(bx - 6, mouthY + 4, bx, mouthY, CLR_BUNNY_OUTLINE);
    _tft.drawLine(bx, mouthY, bx + 6, mouthY + 4, CLR_BUNNY_OUTLINE);
    // Open mouth fill (small filled ellipse)
    _tft.fillEllipse(bx, mouthY + 3, 4, 3, 0xA490);
  }

  // ── Sweat drop indicator (panic, alternates per second) ──────────────
  if (isPanic)
  {
    // Two alternating single-pixel sweat drops
    bool sweatA = ((int)_gaugeAnimT % 2 == 0);
    if (sweatA)
    {
      _tft.fillCircle(bx + 22, by - 12, 3, 0x867F); // light blue
      _tft.drawLine(bx + 22, by - 15, bx + 22, by - 18, 0x867F);
    }
    else
    {
      _tft.fillCircle(bx - 22, by - 10, 3, 0x867F);
      _tft.drawLine(bx - 22, by - 13, bx - 22, by - 16, 0x867F);
    }
  }

  // ── Zzz (very early in task, relax only — urgency < 0.2) ─────────────
  if (isRelax && urgency < 0.2f)
  {
    // Alternating single z pixels that appear/fade
    bool zOn = ((int)_gaugeAnimT % 3 == 0);
    if (zOn)
    {
      _tft.setTextColor(CLR_ACCENT, CLR_BG);
      _tft.setTextSize(1);
      _tft.setCursor(bx + 22, by - 20);
      _tft.print("z");
    }
    bool z2On = ((int)_gaugeAnimT % 3 == 1);
    if (z2On)
    {
      _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
      _tft.setTextSize(1);
      _tft.setCursor(bx + 26, by - 26);
      _tft.print("z");
    }
  }
}

// Legacy stub — kept so existing call sites compile.
// New code should call _drawGauge() directly.
void DisplayManager::_drawBunny(int barX, int barY, int barW,
                                float progressPct,
                                int remainingSec, int totalSec)
{
  // No-op — replaced by _drawGauge() in _drawCurrentTask()
  (void)barX;
  (void)barY;
  (void)barW;
  (void)progressPct;
  (void)remainingSec;
  (void)totalSec;
}

void DisplayManager::setScreen(Screen s)
{
  _screen = s;
  _tft.fillScreen(CLR_BG);
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch polling
// Returns: -1=no touch, 0=main area, 1=All Tasks, 2=Mute,
//           3=Back (task list), 4=Weather row (clothing overlay)
// ─────────────────────────────────────────────────────────────────────────────
int DisplayManager::pollTouch()
{
  uint16_t tx, ty;
  if (!_tft.getTouch(&tx, &ty))
    return -1;

  if (_screen == Screen::TASK_LIST)
  {
    if (ty < 30)
      return 3; // Back button
    return 0;
  }

  // HOME screen zones
  if (ty >= 20 && ty < 60)
    return 4; // Weather row → clothing overlay
  if (ty > 262)
  { // Nav bar
    return (tx < 120) ? 1 : 2;
  }

  return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clothing overlay — draws over the full screen, blocks until tap or timeout,
// then redraws the current screen.
// ─────────────────────────────────────────────────────────────────────────────
#include "ClothingAdvisor.h"

void DisplayManager::showClothingOverlay()
{
  if (!weather.valid)
    return; // No data — nothing to recommend

  auto items = ClothingAdvisor::recommend(weather);
  _drawClothingOverlay(items);

  // Block until tap or 8-second auto-dismiss
  uint32_t start = millis();
  while (millis() - start < 8000)
  {
    uint16_t tx, ty;
    if (_tft.getTouch(&tx, &ty))
    {
      delay(200); // debounce
      break;
    }
    delay(50);
  }

  // Redraw home screen
  time_t t = time(nullptr);
  struct tm *tm_now = localtime(&t);
  update(tm_now);
}

void DisplayManager::_drawClothingOverlay(const std::vector<ClothingItem> &items)
{
  _tft.fillRect(0, 0, 240, 320, CLR_BG);

  // ── Header bar ───────────────────────────────────────────────────────────
  _tft.fillRect(0, 0, 240, 28, CLR_STATUSBG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.setTextSize(1);
  _tft.drawString("What to Wear", 100, 10);

  _tft.setTextDatum(TR_DATUM);
  _tft.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _tft.drawString("tap to close", 234, 10);

  // ── Conditions summary ───────────────────────────────────────────────────
  auto &cfg = Config::instance();
  char summary[40];
  snprintf(summary, sizeof(summary), "%.0f%s  %s",
           weather.temp,
           cfg.data.metricUnits ? "\xF7"
                                  "C"
                                : "\xF7"
                                  "F",
           weather.description.substring(0, 16).c_str());
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
  _tft.setTextSize(1);
  _tft.drawString(summary, 120, 40);
  _tft.setTextDatum(TL_DATUM);

  // ── Item list ────────────────────────────────────────────────────────────
  int y = 54;
  int maxItems = min((int)items.size(), 7);

  for (int i = 0; i < maxItems; i++)
  {
    uint32_t rowBg = (i % 2 == 0) ? CLR_CARD : CLR_BG;
    _tft.fillRect(0, y, 240, 30, rowBg);
    _tft.fillRect(0, y, 3, 30, CLR_ACCENT); // Left accent bar

    // Icon (size 2 = ~16px)
    _tft.setTextSize(2);
    _tft.setTextColor(CLR_TEXT, rowBg);
    _tft.setCursor(10, y + 7);
    _tft.print(items[i].icon.c_str());

    // Label
    _tft.setTextSize(1);
    _tft.setCursor(36, y + 11);
    _tft.print(items[i].label.substring(0, 26).c_str());

    y += 32;
  }

  // Overflow indicator
  if ((int)items.size() > maxItems)
  {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(
        ("+" + String(items.size() - maxItems) + " more").c_str(),
        120, y + 6);
    _tft.setTextDatum(TL_DATUM);
  }

  // ── Dismiss hint ─────────────────────────────────────────────────────────
  _tft.fillRect(0, 300, 240, 20, CLR_STATUSBG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _tft.drawString("Tap anywhere to dismiss", 120, 310);
  _tft.setTextDatum(TL_DATUM);
}