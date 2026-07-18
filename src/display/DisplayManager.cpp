#include "DisplayManager.h"
#include <ctime>
using std::difftime;
using std::gmtime;
using std::localtime;
using std::mktime;
using std::strftime;
using std::time;
using std::time_t;
using std::tm;
#include <ctime>
#include <algorithm>
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

void DisplayManager::update(std::tm *now)
{
  if (_screen == Screen::TIMER_SET)
  {
    _drawTimerSet();
    return;
  }
  if (_screen == Screen::TIMER_RUNNING)
  {
    _drawTimerRunning();
    return;
  }
  // 1. Draw your status bar at the top
  _drawStatusBar(now);

  // 2. Refresh the weather row layout
  _drawWeatherRow();

  // 3. Render whatever view/card is active on the screen
  if (_screen == Screen::HOME)
  {
    _drawCurrentTask();
  }
  else if (_screen == Screen::TASK_LIST)
  {
    _drawTaskList();
  }
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

  int fillW = std::max(1, (int)(pct / 100.0f * 20));
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
void DisplayManager::_drawStatusBar(std::tm *now)
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
// Current Task Card — dial layout
//
// Screen pixel budget (portrait 240×320):
//   y=00–20:  Status bar
//   y=20–58:  Weather row (tap → clothing overlay)
//   y=58–74:  Group label  — size 1 (10px), centred, uppercase, dim
//   y=84–104: Task name    — size 2 (16px bold), centred
//   y=110–238: Dial arc    — r=64, cx=120, cy=174
//             └─ Emoji centred inside arc (bob ±2px)
//             └─ Time remaining below emoji
//             └─ "of X min" sub-label
//   y=240–268: Next task strip — height 28px, font size 1 (larger padding)
//   y=268–320: Nav bar
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawCurrentTask()
{
  _tft.fillRect(0, 58, 240, 210, CLR_BG);

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

  const float urgency = (t->durationMin > 0)
                            ? 1.0f - ((float)sched.remainingSec() / (float)(t->durationMin * 60))
                            : 1.0f;

  uint32_t arcColor;
  if (urgency > 0.8f)
  {
    arcColor = ((int)_gaugeAnimT % 2 == 0) ? CLR_RED : 0xA800; // pulse
  }
  else if (urgency > 0.5f)
  {
    arcColor = CLR_YELLOW;
  }
  else
  {
    arcColor = CLR_ACCENT;
  }

  // ── Group label — size 1, centred, y=62 ──────────────────────────────────
  if (sched.currentGroup())
  {
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.drawString(sched.currentGroup()->name.c_str(), 120, 62);
  }

  // ── Task name — size 2, centred, y=84 (22px gap below group) ─────────────
  _tft.setTextColor(CLR_TEXT, CLR_BG);
  _tft.setTextSize(2);
  _tft.setTextDatum(TC_DATUM);
  _tft.drawString(t->name.substring(0, 14).c_str(), 120, 84);
  _tft.setTextDatum(TL_DATUM);

  // ── Dial — cx=120, cy=174, r=64 ──────────────────────────────────────────
  _drawDial(120, 174, 64,
            sched.progressPct(), sched.remainingSec(), t->durationMin * 60,
            urgency, arcColor);

  // ── Next task strip — y=240, height=28px ─────────────────────────────────
  const Task *next = sched.nextTask();
  _tft.fillRect(0, 240, 240, 28, CLR_CARD);
  _tft.drawFastHLine(0, 240, 240, CLR_SUBTEXT);
  if (next)
  {
    _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _tft.setTextSize(1);
    _tft.setCursor(10, 251);
    _tft.print("NEXT: ");
    _tft.setTextColor(CLR_TEXT, CLR_CARD);
    _tft.print(next->name.substring(0, 18).c_str());
    // Start time right-aligned
    char ntBuf[8];
    snprintf(ntBuf, sizeof(ntBuf), "%02d:%02d", next->startHour, next->startMin);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _tft.drawString(ntBuf, 232, 251);
    _tft.setTextDatum(TL_DATUM);
  }
}
// ─────────────────────────────────────────────────────────────────────────────
// Dial timer
//
// 270° arc ring (225°→495°) showing time remaining. Arc sweeps from 8-o'clock
// clockwise. Colour: cyan (>50% left) → yellow (20–50%) → red pulsing (<20%).
//
// Inside the dial:
//   - Mood emoji (😊/😰/😱) centred, slow bob ±2px at 1.6 rad/s
//   - Time remaining in large bold text below emoji, coloured by urgency
//   - "of X min" in dim grey below that
//
// cx=120, cy=174, r=64 fits exactly between task name (y=84) and
// next-task strip (y=240) with 6px clear each side.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawDial(int cx, int cy, int r,
                               float progressPct, int remainingSec, int totalSec,
                               float urgency, uint32_t arcColor)
{
  const int STROKE = 10;
  const int r_arc = r - STROKE / 2;

  // ── Track ring ────────────────────────────────────────────────────────────
  _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                     225, 495, CLR_GAUGE_BG, CLR_BG);

  // ── Remaining arc ─────────────────────────────────────────────────────────
  if (urgency < 1.0f)
  {
    float endDeg = 225.0f + 270.0f * (1.0f - urgency);
    _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                       225, (int)endDeg, arcColor, CLR_BG);
  }

  // ── Emoji — slow bob ±2px ─────────────────────────────────────────────────
  int bobY = (int)(sinf(_gaugeAnimT * 1.6f) * 2.0f);

  // ASCII fallback faces (replace with NotoEmoji VLW for real emoji — see note)
  const char *face = (urgency > 0.8f)   ? ":o"
                     : (urgency > 0.5f) ? ":/"
                                        : ":)";
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextSize(3);
  _tft.setTextColor(arcColor, CLR_BG);
  _tft.drawString(face, cx, cy - 14 + bobY);

  // ── Time remaining ────────────────────────────────────────────────────────
  int remM = remainingSec / 60;
  int remS = remainingSec % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", remM, remS);
  _tft.setTextSize(2);
  _tft.setTextColor(arcColor, CLR_BG);
  _tft.drawString(timeBuf, cx, cy + 16 + bobY);

  // ── "of X min" ────────────────────────────────────────────────────────────
  _tft.setTextSize(1);
  _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
  char ofBuf[12];
  snprintf(ofBuf, sizeof(ofBuf), "of %d min", totalSec / 60);
  _tft.drawString(ofBuf, cx, cy + 32);
  _tft.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Emoji VLW font upgrade path (NotoEmoji)
//
// ASCII faces (:) :/ :o) work with no extra files. For real 😊 😰 😱:
//   1. Download NotoEmoji-Regular.ttf (fonts.google.com)
//   2. Convert: Processing TFT_eSPI font tool, 22pt,
//      codepoints U+1F60A U+1F630 U+1F631 → NotoEmoji22.vlw (~14KB)
//   3. Add to data/ and upload via LittleFS
//   4. In _drawDial(), replace the ASCII block with:
//      _tft.loadFont("NotoEmoji22", LittleFS);
//      const char* e = urgency > 0.8f ? "\xF0\x9F\x98\xB1"   // 😱
//                     : urgency > 0.5f ? "\xF0\x9F\x98\xB0"  // 😰
//                                      : "\xF0\x9F\x98\x8A"; // 😊
//      _tft.setTextDatum(MC_DATUM);
//      _tft.setTextColor(arcColor, CLR_BG);
//      _tft.drawString(e, cx, cy - 14 + bobY);
//      _tft.unloadFont();
// ─────────────────────────────────────────────────────────────────────────────

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

  if (_screen == Screen::TIMER_SET)
    return _handleTimerSetTouch(tx, ty);
  if (_screen == Screen::TIMER_RUNNING)
    return _handleTimerRunningTouch(tx, ty);

  // HOME screen zones
  if (ty >= 20 && ty < 60)
    return 4; // Weather row → clothing overlay
  if (ty > 262)
  { // Nav bar, now 3 buttons
    if (tx < 80)
      return 1; // Task List
    if (tx < 160)
      return 5; // Timer (NEW)
    return 2;   // Mute
  }

  return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clothing overlay — draws over the full screen, blocks until tap or timeout,
// then redraws the current screen.
// ─────────────────────────────────────────────────────────────────────────────
#include "ClothingAdvisor.h"
#include <audio/AudioManager.h>

void DisplayManager::showClothingOverlay()
{
  if (!weather.valid)
    return; // No data — nothing to recommend

  auto items = ClothingAdvisor::recommend(weather);
  _drawClothingOverlay(items);

  _overlayActive = true;
  _overlayStart = millis();
}

// Call this every loop() iteration. Non-blocking: checks for a tap or
// timeout, and if either fires, dismisses the overlay and redraws.
void DisplayManager::tickOverlay()
{
  if (!_overlayActive)
    return;

  uint16_t tx, ty;
  bool tapped = _tft.getTouch(&tx, &ty);
  bool timedOut = (millis() - _overlayStart) >= 8000;

  if (tapped || timedOut)
  {
    _overlayActive = false;

    time_t t = time(nullptr);
    std::tm *tm_now = localtime(&t);
    this->update(tm_now);
  }
}

void DisplayManager::_drawTaskList()
{
  _tft.fillRect(0, 0, 240, 320, CLR_BG);

  // Simple header with a back affordance (matches pollTouch's ty<30 zone)
  _tft.fillRect(0, 0, 240, 30, CLR_STATUSBG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.drawString("All Tasks", 120, 15);
  _tft.setTextDatum(TL_DATUM);

  int y = 30 - _scrollOffset; // scroll shifts content up
  auto &sched = TaskScheduler::instance();

  for (size_t gi = 0; gi < sched.groups.size(); gi++)
  {
    auto &grp = sched.groups[gi];

    if (y > -24 && y < 320)
    { // only draw if roughly on-screen
      _tft.fillRect(0, y, 240, 24, CLR_CARD);
      _tft.setTextColor(CLR_TEXT, CLR_CARD);
      _tft.setTextSize(1);
      _tft.setCursor(10, y + 8);
      _tft.print(grp.name.c_str());
      _tft.setCursor(220, y + 8);
      _tft.print(_groupExpanded[gi] ? "-" : "+");
    }
    y += 24;

    if (!_groupExpanded[gi])
      continue;

    for (auto &task : grp.tasks)
    {
      if (y > -20 && y < 320)
      {
        _tft.fillRect(0, y, 240, 20, CLR_BG);
        _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
        _tft.setCursor(20, y + 5);
        char line[40];
        snprintf(line, sizeof(line), "%02d:%02d  %s",
                 task.startHour, task.startMin, task.name.c_str());
        _tft.print(line);
      }
      y += 20;
    }
  }

  _contentHeight = y + _scrollOffset - 30; // total scrollable height
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
  int maxItems = std::min((int)items.size(), 7);

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

void DisplayManager::_handleTaskListTap(int ty)
{
  int y = 30 - _scrollOffset; // must mirror the layout math in _drawTaskList()
  auto &sched = TaskScheduler::instance();

  for (size_t gi = 0; gi < sched.groups.size(); gi++)
  {
    if (ty >= y && ty < y + 24)
    {
      _groupExpanded[gi] = !_groupExpanded[gi];
      _screenDirty = true;
      return;
    }
    y += 24;
    if (_groupExpanded[gi])
      y += (int)sched.groups[gi].tasks.size() * 20;
  }
}

void DisplayManager::updateTaskListScroll()
{
  if (_screen != Screen::TASK_LIST)
  {
    _touchDown = false;
    return;
  }

  uint16_t tx, ty;
  bool touched = _tft.getTouch(&tx, &ty);

  if (touched && !_touchDown)
  {
    // Case 1: finger just landed this frame — start of a new gesture
    _touchDown = true;
    _isDragging = false;
    _touchStartY = (int)ty;
    _lastTouchY = (int)ty;
  }
  else if (touched && _touchDown)
  {
    // Case 2: finger still down — check if it has moved enough to count as a drag
    if (!_isDragging && abs((int)ty - _touchStartY) > 6)
    {
      _isDragging = true; // crossed the threshold — this is now a drag, not a tap
    }

    if (_isDragging)
    {
      int delta = _lastTouchY - (int)ty;
      if (delta != 0)
      {
        int maxScroll = std::max(0, _contentHeight - (320 - 30));
        _scrollOffset = constrain(_scrollOffset + delta, 0, maxScroll);
        _screenDirty = true;
      }
    }
    _lastTouchY = (int)ty;
  }
  else if (!touched && _touchDown)
  {
    // Case 3: finger just lifted — decide what kind of gesture this was
    if (!_isDragging)
    {
      _handleTaskListTap(_touchStartY); // it never moved much -> treat as a tap
    }
    _touchDown = false;
    _isDragging = false;
  }
}

// ── Manual timer ──────────────────────────────────────────────────────────
void DisplayManager::startTimer(uint32_t seconds)
{
  _timerDurationSec = seconds;
  _timerStartMillis = millis();
  _timerRunning = true;
  _timerDone = false;
  setScreen(Screen::TIMER_RUNNING);
}

void DisplayManager::_drawTimerSet()
{
  _tft.fillRect(0, 0, 240, 320, CLR_BG);
  _tft.fillRect(0, 0, 240, 30, CLR_STATUSBG);
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _tft.drawString("Set Timer", 120, 15);

  const char *labels[6] = {"1 min", "5 min", "10 min", "15 min", "20 min", "30 min"};
  const int cols = 2, btnW = 104, btnH = 56, gapX = 8, gapY = 10;
  const int startX = (240 - (cols * btnW + gapX)) / 2, startY = 50;

  for (int i = 0; i < 6; i++)
  {
    int col = i % cols, row = i / cols;
    int x = startX + col * (btnW + gapX), y = startY + row * (btnH + gapY);
    _tft.fillRoundRect(x, y, btnW, btnH, 8, CLR_CARD);
    _tft.drawRoundRect(x, y, btnW, btnH, 8, CLR_SUBTEXT);
    _tft.setTextColor(CLR_TEXT, CLR_CARD);
    _tft.setTextSize(2);
    _tft.drawString(labels[i], x + btnW / 2, y + btnH / 2);
  }
  _tft.setTextSize(1);
  _tft.setTextDatum(TL_DATUM);
}

int DisplayManager::_handleTimerSetTouch(uint16_t tx, uint16_t ty)
{
  if (ty < 30)
  {
    setScreen(Screen::HOME);
    return -1;
  }

  static const uint32_t presetsSec[6] = {60, 300, 600, 900, 1200, 1800};
  const int cols = 2, btnW = 104, btnH = 56, gapX = 8, gapY = 10;
  const int startX = (240 - (cols * btnW + gapX)) / 2, startY = 50;

  for (int i = 0; i < 6; i++)
  {
    int col = i % cols, row = i / cols;
    int x = startX + col * (btnW + gapX), y = startY + row * (btnH + gapY);
    if (tx >= x && tx < x + btnW && ty >= y && ty < y + btnH)
    {
      startTimer(presetsSec[i]);
      return -1;
    }
  }
  return -1;
}

void DisplayManager::_drawTimerRunning()
{
  _tft.fillRect(0, 0, 240, 320, CLR_BG);

  uint32_t elapsed = (millis() - _timerStartMillis) / 1000;
  uint32_t remaining = (elapsed >= _timerDurationSec) ? 0 : (_timerDurationSec - elapsed);

  if (remaining == 0 && !_timerDone)
  {
    _timerDone = true;
    _timerRunning = false;
    if (!Config::instance().data.muted)
    {
      for (int i = 0; i < 3; i++)
      {
        AudioManager::instance().beep(1500, 40);
        delay(120);
      }
    }
  }

  _tft.setTextDatum(MC_DATUM);

  if (_timerDone)
  {
    _tft.setTextColor(CLR_ACCENT, CLR_BG);
    _tft.setTextSize(3);
    _tft.drawString("Time's Up!", 120, 130);
    _tft.setTextSize(1);
    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.drawString("Tap below to dismiss", 120, 170);
  }
  else
  {
    uint32_t mm = remaining / 60, ss = remaining % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
    _tft.setTextColor(CLR_TEXT, CLR_BG);
    _tft.setTextSize(5);
    _tft.drawString(buf, 120, 130);
    _tft.setTextSize(1);

    float pct = 1.0f - (float)remaining / (float)_timerDurationSec;
    const int barW = 200, barH = 10, barX = 20, barY = 180;
    _tft.drawRoundRect(barX, barY, barW, barH, 4, CLR_SUBTEXT);
    _tft.fillRoundRect(barX + 1, barY + 1, (int)((barW - 2) * pct), barH - 2, 3, CLR_ACCENT);

    _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
    _tft.drawString("Tap below to cancel", 120, 210);
  }

  _tft.fillRoundRect(60, 260, 120, 40, 8, CLR_CARD);
  _tft.drawRoundRect(60, 260, 120, 40, 8, CLR_RED);
  _tft.setTextColor(CLR_RED, CLR_CARD);
  _tft.drawString(_timerDone ? "OK" : "Cancel", 120, 280);
  _tft.setTextDatum(TL_DATUM);
}

int DisplayManager::_handleTimerRunningTouch(uint16_t tx, uint16_t ty)
{
  if (ty >= 260 && ty < 300 && tx >= 60 && tx < 180)
  {
    _timerRunning = false;
    _timerDone = false;
    setScreen(Screen::HOME);
  }
  return -1;
}