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
#include <cmath>
#include <WiFi.h>
#include "../config/Config.h"
#include "../sensors/BatteryMonitor.h"

// Compact 12-hour formatter for secondary time references (next-task strip,
// task list rows) -- "A"/"P" instead of full "AM"/"PM" to save width those
// tighter layouts don't have. The primary clock in the status bar still
// spells out the full "AM"/"PM" (see _drawStatusBar).
static void format12hShort(int hour24, int minute, char *buf, size_t n)
{
  int h = hour24 % 12;
  if (h == 0)
    h = 12;
  snprintf(buf, n, "%d:%02d%s", h, minute, hour24 < 12 ? "A" : "P");
}

DisplayManager &DisplayManager::instance()
{
  static DisplayManager inst;
  return inst;
}

void DisplayManager::begin()
{
  // Real hardware only addresses correctly at rotation 1 (320x240) on this
  // panel -- see the big comment block in DisplayManager.h. Everything we
  // draw still happens in the original 240x320 logical space via _sprite;
  // _tft itself is never drawn on directly.
  _tft.init();
  _tft.setRotation(1);
  _tft.fillScreen(TFT_BLACK); // blank the real panel immediately at boot

  // Must happen in this rotation, before anything else touch-related --
  // see the big comment on _initTouchCalibration() in the header for why.
  _initTouchCalibration();

  // pushRotated() centers the rotated sprite on the REAL screen at whatever
  // pivot is set on the real _tft object -- this is separate from the
  // sprite's own pivot (below) and defaults to (0,0), the top-left corner,
  // if never set. That was the bug: without this call, the whole rotated
  // frame was being centered at the screen's top-left instead of its
  // middle, shoving almost everything off-canvas.
  _tft.setPivot(_tft.width() / 2, _tft.height() / 2); // real screen center, e.g. (160,120) at rotation 1

  _sprite.setColorDepth(16); // match the app's existing RGB565 color constants exactly
  void *buf = _sprite.createSprite(SPRITE_W, SPRITE_H);
  _spriteOk = (buf != nullptr);
  if (!_spriteOk)
  {
    Serial.println("[Display] FATAL: sprite allocation failed (out of RAM?). "
                    "Nothing will render -- see DisplayManager.h comment re: "
                    "shrinking to setColorDepth(8) or per-region sub-sprites.");
    return;
  }
  _sprite.setPivot(SPRITE_W / 2, SPRITE_H / 2); // rotation center WITHIN the sprite (its own middle)
  _sprite.setTextDatum(TL_DATUM);
  _sprite.fillSprite(CLR_BG);

  for (int i = 0; i < 16; i++)
    _groupExpanded[i] = true; // All expanded by default
}

// ─────────────────────────────────────────────────────────────────────────────
// Loads _touchCal from NVS -- that's ALL this does now. Calibration itself
// is produced by the separate ../touch_calibration_tool project (uses
// TFT_eSPI's own tft.calibrateTouch(), writes to the same NVS namespace/key
// this reads). See the header comment above this declaration for why
// calibration doesn't happen inline in this firmware at all anymore.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_initTouchCalibration()
{
  Serial.println("[TouchCal] start");

  Preferences prefs;
  bool prefsOk = prefs.begin("touchcal", false);
  Serial.printf("[TouchCal] prefs.begin() = %d\n", prefsOk);

  size_t got = prefs.getBytes("cal", _touchCal, sizeof(_touchCal));
  bool haveCal = (got == sizeof(_touchCal)) &&
                 !(_touchCal[0] == 0 && _touchCal[1] == 0 && _touchCal[2] == 0 &&
                   _touchCal[3] == 0 && _touchCal[4] == 0);
  Serial.printf("[TouchCal] stored bytes read = %u, haveCal = %d\n", (unsigned)got, haveCal);
  prefs.end();

  if (!haveCal)
  {
    // Rough, uncalibrated-but-nonzero fallback (typical raw ADC range for
    // this touch controller) so touch stays at least coarsely usable
    // instead of _getRuntimeTouch()'s divide-by-zero guard disabling it
    // completely. Run ../touch_calibration_tool once to replace this with
    // a real calibration.
    Serial.println("[TouchCal] no saved calibration found -- using rough fallback "
                    "constants (300-3600 raw range, no rotate/invert). Run the "
                    "touch_calibration_tool project once, then reflash this "
                    "firmware, to get a real calibration.");
    _touchCal[0] = 300;
    _touchCal[1] = 3300;
    _touchCal[2] = 300;
    _touchCal[3] = 3300;
    _touchCal[4] = 0;
  }

  _tft.setTouch(_touchCal);
  Serial.println("[TouchCal] done, setTouch() applied");
}

// ─────────────────────────────────────────────────────────────────────────────
// Ongoing touch reads, bypassing _tft.getTouch()/validTouch() entirely -- see
// the header comment above the declaration for why. Single-shot, non-
// blocking (returns false immediately if nothing currently qualifies -- the
// caller already polls every loop() iteration, so there's no need to wait
// here the way calibration does). Applies our own copy of TFT_eSPI's
// calibration math using the stored _touchCal array.
// ─────────────────────────────────────────────────────────────────────────────
bool DisplayManager::_getRuntimeTouch(uint16_t &screenX, uint16_t &screenY)
{
  // Guard against a division by zero if calibration never succeeded AND the
  // fallback-constants assignment in _initTouchCalibration() somehow didn't
  // run either -- shouldn't happen anymore (see that function), but this
  // stays as a last-resort safety net rather than risking UB.
  if (_touchCal[1] == 0 || _touchCal[3] == 0)
  {
    static uint32_t lastWarn = 0;
    if (millis() - lastWarn > 5000)
    {
      lastWarn = millis();
      Serial.println("[Touch] _touchCal has a zero span -- ongoing touch reads "
                      "disabled (this shouldn't happen; check _initTouchCalibration)");
    }
    return false;
  }

  const uint16_t Z_MIN = 110; // loosened from the library default of 350 for this panel's marginal touch signal

  uint16_t z1 = _tft.getTouchRawZ();
  if (z1 <= Z_MIN)
    return false;

  uint16_t rx, ry;
  _tft.getTouchRaw(&rx, &ry);

  // Loose single re-check rather than the library's strict "two samples
  // within 20 raw units" agreement -- that exact requirement is what made
  // calibration hang on this panel, so holding ongoing touch reads to it
  // would cause the same "doesn't register" behavior during normal use.
  delay(1);
  uint16_t z2 = _tft.getTouchRawZ();
  if (z2 <= Z_MIN)
  {
    // Logged (not silent) so a serial capture during a "taps aren't
    // registering" session shows whether presses are crossing the first
    // threshold at all, or dying on the confirm step -- two very different
    // problems that look identical from the outside.
    Serial.printf("[Touch] z1=%u crossed threshold but z2=%u didn't confirm -- dropped\n", z1, z2);
    return false;
  }

  bool rotate = (_touchCal[4] & 0x01) != 0;
  bool invX = (_touchCal[4] & 0x02) != 0;
  bool invY = (_touchCal[4] & 0x04) != 0;

  int rawA = rotate ? ry : rx; // raw axis that maps to screen X
  int rawB = rotate ? rx : ry; // raw axis that maps to screen Y

  int32_t xx = (int32_t)(rawA - (int32_t)_touchCal[0]) * _tft.width() / (int32_t)_touchCal[1];
  int32_t yy = (int32_t)(rawB - (int32_t)_touchCal[2]) * _tft.height() / (int32_t)_touchCal[3];

  xx = constrain(xx, 0, (int32_t)_tft.width() - 1);
  yy = constrain(yy, 0, (int32_t)_tft.height() - 1);

  if (invX)
    xx = (int32_t)_tft.width() - 1 - xx;
  if (invY)
    yy = (int32_t)_tft.height() - 1 - yy;

  screenX = (uint16_t)xx;
  screenY = (uint16_t)yy;
  Serial.printf("[Touch] z1=%u z2=%u raw=(%u,%u) -> screen=(%u,%u)\n", z1, z2, rx, ry, screenX, screenY);
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Push the finished _sprite frame onto the real panel, rotated to match this
// panel's working hardware orientation. Call this once per complete frame --
// not per draw call.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_present()
{
  if (!_spriteOk)
    return;
  _sprite.pushRotated(SPRITE_ROTATE_DEG);
}

// ─────────────────────────────────────────────────────────────────────────────
// _getRuntimeTouch() returns physical panel coordinates (320x240, rotation 1)
// -- same space _tft.getTouch() would have returned, just via our own looser
// read/calibration path instead of the library's strict validTouch() (see
// the header comment on _getRuntimeTouch() for why that swap matters).
// Every touch-handling call site in this file expects logical/sprite-space
// coordinates (240x320) -- the same space the drawing code uses. This applies
// the inverse of the rotation _present() applies, so the two stay in sync:
// flipping SPRITE_ROTATE_DEG in the header automatically flips this too.
// ─────────────────────────────────────────────────────────────────────────────
bool DisplayManager::_getLogicalTouch(uint16_t &lx, uint16_t &ly)
{
  uint16_t px, py;
  if (!_getRuntimeTouch(px, py))
    return false;

  if (SPRITE_ROTATE_DEG == 90)
  {
    lx = py;
    ly = SPRITE_H - 1 - px;
  }
  else // 270
  {
    lx = SPRITE_W - 1 - py;
    ly = px;
  }
  return true;
}

void DisplayManager::update(std::tm *now)
{
  if (!_spriteOk)
    return;

  // Drives the dial's urgency pulse and the weather icon's animation. This
  // was declared but never incremented before -- both effects were frozen
  // on their very first frame the whole time. update() runs ~once/second
  // (see main.cpp's 1-second tick), so +1.0 here is "elapsed seconds".
  _gaugeAnimT += 1.0f;

  if (_screen == Screen::TIMER_SET)
  {
    _drawTimerSet();
    _present();
    return;
  }
  if (_screen == Screen::TIMER_RUNNING)
  {
    _drawTimerRunning();
    _present();
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
    _drawNavBar();
  }
  else if (_screen == Screen::TASK_LIST)
  {
    _drawTaskList();
  }

  _present();
}

// ─────────────────────────────────────────────────────────────────────────────
// Bottom nav bar (y=268..320) — Tasks / Timer / Mute.
// This was declared in the header but never implemented or called, which is
// why none of these ever showed up: the row simply never got drawn. Column
// widths (80px each) and the y>262 threshold match pollTouch()'s existing
// HOME-screen zone logic (1=Tasks, 5=Timer, 2=Mute) further down in this
// file -- only home screen; TASK_LIST/TIMER_SET/TIMER_RUNNING use their own
// full-height layouts and don't call this.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawNavBar()
{
  const int y = 268, h = 320 - y;
  const int midY = y + h / 2;
  bool muted = Config::instance().data.muted;

  _sprite.fillRect(0, y, 240, h, CLR_STATUSBG);
  _sprite.drawFastHLine(0, y, 240, CLR_SUBTEXT);
  _sprite.drawFastVLine(80, y, h, CLR_SUBTEXT);
  _sprite.drawFastVLine(160, y, h, CLR_SUBTEXT);

  // Icons only, no labels, drawn from primitives -- there's no icon font
  // compiled in (only plain bitmap text), so these are hand-drawn shapes.

  // ── Tasks: a small 3-row checklist (green) ────────────────────────────────
  {
    const int cx = 40;
    for (int row = -1; row <= 1; row++)
    {
      int ry = midY + row * 7;
      _sprite.drawRect(cx - 13, ry - 2, 5, 5, CLR_GREEN);
      _sprite.drawFastHLine(cx - 5, ry, 13, CLR_GREEN);
    }
  }

  // ── Timer: a clock face with hands and a top stem (orange) ───────────────
  {
    const int cx = 120;
    _sprite.drawCircle(cx, midY, 10, CLR_ORANGE);
    _sprite.fillRect(cx - 1, midY - 13, 3, 3, CLR_ORANGE); // stem/button
    _sprite.drawLine(cx, midY, cx, midY - 6, CLR_ORANGE);  // minute hand
    _sprite.drawLine(cx, midY, cx + 5, midY, CLR_ORANGE);  // hour hand
  }

  // ── Mute: speaker cone, plus sound-wave chevrons or a slash when muted ────
  {
    const int cx = 200;
    uint32_t color = muted ? CLR_YELLOW : CLR_ACCENT;
    _sprite.fillRect(cx - 14, midY - 3, 4, 6, color);
    _sprite.fillTriangle(cx - 10, midY - 3, cx - 10, midY + 3, cx - 2, midY + 8, color);
    _sprite.fillTriangle(cx - 10, midY - 3, cx - 2, midY + 8, cx - 2, midY - 8, color);
    if (muted)
    {
      _sprite.drawLine(cx - 14, midY - 10, cx + 6, midY + 10, CLR_RED);
      _sprite.drawLine(cx - 14, midY + 10, cx + 6, midY - 10, CLR_RED);
    }
    else
    {
      _sprite.drawLine(cx + 2, midY - 6, cx + 8, midY, color);
      _sprite.drawLine(cx + 8, midY, cx + 2, midY + 6, color);
      _sprite.drawLine(cx + 6, midY - 10, cx + 14, midY, color);
      _sprite.drawLine(cx + 14, midY, cx + 6, midY + 10, color);
    }
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
    _sprite.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, CLR_RED);
    _sprite.drawLine(cx + 5, cy - 5, cx - 5, cy + 5, CLR_RED);
    return;
  }

  // 4 vertical bars, shortest→tallest left→right, baseline-aligned — the
  // familiar phone/wifi signal-strength meter. Replaces the old concentric
  // arc "fan", which came out oriented wrong on this panel.
  const int barW = 3, gap = 2;
  const int heights[4] = {4, 7, 10, 13};
  const int baseline = cy + 7; // bottom edge shared by all bars
  const int totalW = 4 * barW + 3 * gap;
  int x = cx - totalW / 2;

  for (int i = 0; i < 4; i++)
  {
    uint32_t color = (i < bars) ? CLR_ACCENT : CLR_SUBTEXT;
    _sprite.fillRect(x, baseline - heights[i], barW, heights[i], color);
    x += barW + gap;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Battery Icon
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawBatteryIcon(int x, int y, int pct)
{
  uint32_t fillColor = pct > 50 ? CLR_GREEN : (pct > 20 ? CLR_YELLOW : CLR_RED);

  _sprite.drawRect(x, y, 22, 10, CLR_TEXT);
  _sprite.fillRect(x + 22, y + 3, 2, 4, CLR_TEXT); // nub

  int fillW = std::max(1, (int)(pct / 100.0f * 20));
  _sprite.fillRect(x + 1, y + 1, fillW, 8, fillColor);

  // Right-aligned to a fixed edge instead of a left cursor after the icon --
  // a left cursor let the text's right edge drift depending on whether pct
  // was 2 or 3 digits, which is why the widget never looked flush against
  // the screen edge no matter where x was set.
  char buf[6];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  _sprite.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _sprite.setTextSize(1);
  _sprite.setTextDatum(TR_DATUM);
  _sprite.drawString(buf, 236, y + 1);
  _sprite.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Progress Bar
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color)
{
  _sprite.fillRect(x, y, w, h, CLR_BAR_BG);
  int filled = (int)(pct / 100.0f * w);
  if (filled > 0)
    _sprite.fillRect(x, y, filled, h, color);
  // Rounded end cap
  if (filled > 0 && filled < w)
    _sprite.fillCircle(x + filled, y + h / 2, h / 2, color);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status Bar (top STATUS_BAR_H px — grown from the original 20px to fit a
// bigger clock; WiFi/battery icon anchors and the date's y both moved down
// to stay centered in the taller bar).
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawStatusBar(std::tm *now)
{
  _sprite.fillRect(0, 0, 240, STATUS_BAR_H, CLR_STATUSBG);

  // WiFi bars — left side
  int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
  _drawWifiArcs(14, 10, rssi);

  // Battery — right side. Box position is fixed; the percentage text next
  // to it is now right-aligned to the screen edge inside _drawBatteryIcon
  // itself, so the whole widget reads as flush-right regardless of digits.
  _drawBatteryIcon(184, 7, BatteryMonitor::instance().percentage());

  // Time — center, 12-hour with AM/PM, bigger per request. Computed by hand
  // rather than via strftime's %I/%l: %I zero-pads (e.g. "01:05"), and %l
  // (no leading zero) isn't reliably supported by the ESP32 toolchain's libc.
  int hour12 = now->tm_hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", hour12, now->tm_min,
           now->tm_hour < 12 ? "AM" : "PM");
  _sprite.setTextDatum(TC_DATUM);
  _sprite.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _sprite.setTextSize(2);
  _sprite.drawString(timeBuf, 120, 2);

  // Date — below time, bumped a size too
  char dateBuf[12];
  strftime(dateBuf, sizeof(dateBuf), "%a %d %b", now);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _sprite.setTextSize(1);
  _sprite.drawString(dateBuf, 120, 19);

  _sprite.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Weather Row (y=STATUS_BAR_H to y=STATUS_BAR_H+WEATHER_ROW_H)
// Shows error messages for specific failure modes (bad key, bad city, etc.)
// so the user knows to visit /config rather than just seeing a blank row.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWeatherRow()
{
  const int top = STATUS_BAR_H;
  _sprite.fillRect(0, top, 240, WEATHER_ROW_H, CLR_CARD);

  if (!weather.valid)
  {
    _sprite.setTextDatum(MC_DATUM);
    _sprite.setTextSize(2);

    if (weather.errorMsg == "API key invalid")
    {
      _sprite.setTextColor(CLR_RED, CLR_CARD);
      _sprite.drawString("Bad API key", 120, top + 12);
      _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _sprite.setTextSize(1);
      _sprite.drawString("Fix at /config", 120, top + 28);
    }
    else if (weather.errorMsg == "City not found")
    {
      _sprite.setTextColor(CLR_RED, CLR_CARD);
      _sprite.drawString("City not found", 120, top + 12);
      _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _sprite.setTextSize(1);
      _sprite.drawString("Fix at /config", 120, top + 28);
    }
    else if (!weather.errorMsg.isEmpty())
    {
      _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _sprite.drawString(weather.errorMsg.c_str(), 120, top + WEATHER_ROW_H / 2);
    }
    else
    {
      _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _sprite.drawString("Weather unavailable", 120, top + WEATHER_ROW_H / 2);
    }

    _sprite.setTextDatum(TL_DATUM);
    return;
  }

  auto &cfg = Config::instance();
  const char *unit = cfg.data.metricUnits ? "C" : "F";

  _sprite.setTextColor(CLR_TEXT, CLR_CARD);
  _sprite.setTextSize(2);
  _sprite.setCursor(8, top + 8);
  _sprite.printf("%.0f\xF7%s", weather.temp, unit); // degree symbol = 0xF7 in default font

  // Animated condition icon instead of description/humidity/wind text --
  // "images convey the message better than words" per request.
  _drawWeatherIcon(175, top + WEATHER_ROW_H / 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Animated weather icon -- sun / partly-cloudy / cloudy / rain / storm /
// snow / fog, chosen from weather.icon's OpenWeatherMap code prefix, plus a
// small independent "windy" badge when windSpeed crosses a threshold. All
// primitives, no icon font. Animated off _gaugeAnimT, which now actually
// advances once per second (see update()) instead of sitting frozen at 0.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWeatherIcon(int cx, int cy)
{
  if (!weather.valid)
    return;

  String code = weather.icon.length() >= 2 ? weather.icon.substring(0, 2) : String("01");

  auto drawCloud = [&](int ccx, int ccy, uint32_t color)
  {
    _sprite.fillCircle(ccx - 6, ccy + 2, 6, color);
    _sprite.fillCircle(ccx + 2, ccy - 2, 7, color);
    _sprite.fillCircle(ccx + 9, ccy + 2, 5, color);
    _sprite.fillRect(ccx - 6, ccy + 2, 15, 6, color);
  };

  if (code == "01")
  {
    // Clear -- sun with slowly rotating rays
    float phase = _gaugeAnimT * 0.4f;
    for (int i = 0; i < 8; i++)
    {
      float ang = phase + i * (PI / 4.0f);
      int x1 = cx + (int)(cosf(ang) * 9), y1 = cy + (int)(sinf(ang) * 9);
      int x2 = cx + (int)(cosf(ang) * 13), y2 = cy + (int)(sinf(ang) * 13);
      _sprite.drawLine(x1, y1, x2, y2, CLR_YELLOW);
    }
    _sprite.fillCircle(cx, cy, 7, CLR_YELLOW);
  }
  else if (code == "02")
  {
    // Partly cloudy -- small sun peeking from behind a cloud. Previously
    // fully static; now the peeking sun gets a few short flickering rays
    // (same rotating-ray idea as "01", just fewer/shorter since most of the
    // sun is hidden behind the cloud anyway) so this condition isn't the
    // only "frozen" one on screen.
    int sx = cx - 6, sy = cy - 6;
    float phase = _gaugeAnimT * 0.4f;
    for (int i = 0; i < 4; i++)
    {
      float ang = phase + i * (PI / 2.0f);
      int x1 = sx + (int)(cosf(ang) * 7), y1 = sy + (int)(sinf(ang) * 7);
      int x2 = sx + (int)(cosf(ang) * 10), y2 = sy + (int)(sinf(ang) * 10);
      _sprite.drawLine(x1, y1, x2, y2, CLR_YELLOW);
    }
    _sprite.fillCircle(sx, sy, 6, CLR_YELLOW);
    drawCloud(cx + 2, cy + 2, CLR_CLOUD);
  }
  else if (code == "03" || code == "04")
  {
    // Cloudy -- previously a single static cloud; now drifts gently side to
    // side so overcast/cloudy days (probably the most common condition)
    // aren't the one icon on this screen that never visibly does anything.
    int drift = (int)(sinf(_gaugeAnimT * 0.35f) * 4.0f);
    drawCloud(cx + drift, cy, CLR_CLOUD);
  }
  else if (code == "09" || code == "10")
  {
    // Rain -- cloud with falling drops, looped via fmodf for the drip motion
    drawCloud(cx, cy - 4, CLR_CLOUD);
    for (int i = 0; i < 3; i++)
    {
      int dx = cx - 7 + i * 7;
      int dy = cy + 6 + (int)fmodf(_gaugeAnimT * 6.0f + i * 3, 6.0f);
      _sprite.drawLine(dx, dy, dx - 2, dy + 5, CLR_ACCENT);
    }
  }
  else if (code == "11")
  {
    // Thunderstorm -- cloud with a lightning bolt that flashes
    drawCloud(cx, cy - 4, CLR_CLOUD);
    bool flash = ((int)_gaugeAnimT % 2) == 0;
    uint32_t boltColor = flash ? CLR_YELLOW : CLR_SUBTEXT;
    _sprite.fillTriangle(cx - 2, cy + 5, cx + 3, cy + 5, cx - 1, cy + 11, boltColor);
    _sprite.fillTriangle(cx - 1, cy + 11, cx + 4, cy + 5, cx + 1, cy + 15, boltColor);
  }
  else if (code == "13")
  {
    // Snow -- cloud with drifting flakes
    drawCloud(cx, cy - 4, CLR_CLOUD);
    for (int i = 0; i < 3; i++)
    {
      int fx = cx - 7 + i * 7;
      int fy = cy + 8 + (int)(sinf(_gaugeAnimT + i) * 2);
      _sprite.fillCircle(fx, fy, 1, CLR_TEXT);
    }
  }
  else if (code == "50")
  {
    // Mist/fog -- each line now drifts horizontally at a slightly different
    // phase (was 3 static lines) for a layered "fog rolling past" look.
    for (int i = 0; i < 3; i++)
    {
      int drift = (int)(fmodf(_gaugeAnimT * 5.0f + i * 40.0f, 16.0f)) - 8;
      _sprite.drawFastHLine(cx - 11 + drift, cy - 5 + i * 5, 22, CLR_SUBTEXT);
    }
  }
  else
  {
    // Sensible fallback for any unmapped code -- same gentle drift as the
    // cloudy case so an unrecognized condition string doesn't silently fall
    // back to the one frozen icon on the screen.
    int drift = (int)(sinf(_gaugeAnimT * 0.35f) * 4.0f);
    drawCloud(cx + drift, cy, CLR_CLOUD);
  }

  // ── Windy badge -- small, independent of the main condition icon ─────────
  auto &cfg = Config::instance();
  bool windy = cfg.data.metricUnits ? (weather.windSpeed > 20) : (weather.windSpeed > 12);
  if (windy)
  {
    int wx = cx + 20, wy = cy + 10;
    int off = (int)fmodf(_gaugeAnimT * 8.0f, 5.0f);
    _sprite.drawFastHLine(wx - 6 + off, wy, 7, CLR_ACCENT);
    _sprite.drawFastHLine(wx - 8 + off, wy + 3, 9, CLR_ACCENT);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Largest text size (1..maxSize) whose rendered width fits maxWidthPx, using
// the default GLCD font's fixed 6px-per-character-per-size-step metric --
// used so long task/group names shrink instead of clipping off-screen.
// ─────────────────────────────────────────────────────────────────────────────
int DisplayManager::_fitTextSize(const String &s, int maxSize, int maxWidthPx)
{
  int size = maxSize;
  int len = s.length();
  if (len == 0)
    return maxSize;
  while (size > 1 && len * 6 * size > maxWidthPx)
    size--;
  return size;
}

// ─────────────────────────────────────────────────────────────────────────────
// Current Task Card — dial layout
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawCurrentTask()
{
  const int top = STATUS_BAR_H + WEATHER_ROW_H; // 66 — card starts right after the weather row
  const int bottom = 240 + 28;                  // 268 — next-task strip + dial below didn't need to move
  _sprite.fillRect(0, top, 240, bottom - top, CLR_BG);

  // Shared space-theme backdrop -- twinkling stars behind whichever content
  // this card ends up showing (active task's rocket race, or the idle
  // astronaut scene), so the whole thing reads as one consistent theme.
  _drawStarfield(top, bottom);

  auto &sched = TaskScheduler::instance();
  const Task *t = sched.currentTask();

  if (!t)
  {
    _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.setTextSize(1);
    _sprite.drawString("No active task", 120, top + 20);
    _sprite.setTextDatum(TL_DATUM);
    _drawSleepyAstronaut(120, (top + bottom) / 2 + 20);
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

  // ── Group label — size 1, centred, y=top+4 (accent color for some "pop") ──
  if (sched.currentGroup())
  {
    _sprite.setTextColor(CLR_ACCENT, CLR_BG);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(TC_DATUM);
    _sprite.drawString(sched.currentGroup()->name.c_str(), 120, top + 4);
  }

  // ── Task name — adaptive size so long names shrink instead of clipping ───
  _sprite.setTextColor(CLR_TEXT, CLR_BG);
  _sprite.setTextSize(_fitTextSize(t->name, 2, 228));
  _sprite.setTextDatum(TC_DATUM);
  _sprite.drawString(t->name.c_str(), 120, top + 26);
  _sprite.setTextDatum(TL_DATUM);

  // ── Time-remaining visual — rocket race, centered in the space between
  // the task name and the next-task strip. ─────────────────────────────────
  _drawTimeVisual(120, 200, 95,
                  sched.progressPct(), sched.remainingSec(), t->durationMin * 60,
                  urgency, arcColor);

  // ── Next task strip — y=240, height=28px ─────────────────────────────────
  const Task *next = sched.nextTask();
  _sprite.fillRect(0, 240, 240, 28, CLR_CARD);
  _sprite.drawFastHLine(0, 240, 240, CLR_SUBTEXT);
  if (next)
  {
    _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _sprite.setTextSize(1);
    _sprite.setCursor(10, 251);
    _sprite.print("NEXT: ");
    _sprite.setTextColor(CLR_TEXT, CLR_CARD);
    _sprite.print(next->name.substring(0, 24).c_str());
    // Start time, 12-hour, right-aligned
    char ntBuf[10];
    format12hShort(next->startHour, next->startMin, ntBuf, sizeof(ntBuf));
    _sprite.setTextDatum(TR_DATUM);
    _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
    _sprite.drawString(ntBuf, 232, 251);
    _sprite.setTextDatum(TL_DATUM);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Twinkling starfield -- a fixed set of scattered points within [top, bottom)
// across the full card width, each blinking on its own phase (offset by
// index) so they don't all flash together. Deliberately sparse/small so it
// reads as a backdrop, not something competing with the foreground content
// drawn on top of it afterward.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawStarfield(int top, int bottom)
{
  // Fixed layout (not random per-frame -- randomizing positions every call
  // would make them jitter instead of twinkle). Positions are expressed as
  // fractions of the band so this works for any [top, bottom) range passed
  // in, including the full-screen task-complete animation.
  static const float starX[] = {0.06f, 0.18f, 0.32f, 0.46f, 0.60f, 0.74f, 0.88f,
                                 0.12f, 0.26f, 0.40f, 0.54f, 0.68f, 0.82f, 0.94f};
  static const float starY[] = {0.10f, 0.85f, 0.30f, 0.65f, 0.15f, 0.90f, 0.45f,
                                 0.55f, 0.20f, 0.75f, 0.05f, 0.60f, 0.35f, 0.80f};
  const int numStars = sizeof(starX) / sizeof(starX[0]);
  int h = bottom - top;

  for (int i = 0; i < numStars; i++)
  {
    int sx = (int)(starX[i] * 240.0f);
    int sy = top + (int)(starY[i] * h);
    // Each star's own phase offset (i * 1.7) so they twinkle independently
    // rather than in unison.
    int phase = (int)(_gaugeAnimT * 1.5f + i * 1.7f) % 5;
    if (phase == 0)
      _sprite.fillCircle(sx, sy, 1, CLR_TEXT);   // bright flash
    else if (phase <= 2)
      _sprite.fillCircle(sx, sy, 1, CLR_SUBTEXT); // dim glow
    // else: off entirely this frame
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sleepy astronaut idle animation -- shown for the "no active task" state.
// Replaces the cat (reported as "hideous," most likely the small hand-drawn
// face/ear proportions reading as uncanny at this pixel budget) with a space
// theme matching the rocket-race timer. Deliberately NO face details at all
// -- just an opaque dark visor -- to avoid the same risk. Limbs sway gently
// and the whole figure bobs vertically for a floating-in-zero-g feel; a
// lazy "z" drifts up on a loop, same idea as before.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawSleepyAstronaut(int cx, int cy)
{
  uint32_t suit = CLR_TEXT;
  uint32_t visor = CLR_STATUSBG;

  // Gentle whole-body bob, like drifting in zero gravity
  int bob = (int)(sinf(_gaugeAnimT * 0.3f) * 5.0f);
  cy += bob;

  // Arms -- sway independently, suggesting a loose, weightless drift
  float armSway = sinf(_gaugeAnimT * 0.4f) * 5.0f;
  _sprite.fillRoundRect(cx - 22, cy - 2 + (int)armSway, 9, 18, 4, suit);
  _sprite.fillRoundRect(cx + 13, cy - 2 - (int)armSway, 9, 18, 4, suit);

  // Legs -- slower, smaller sway
  float legSway = sinf(_gaugeAnimT * 0.3f + 1.5f) * 3.0f;
  _sprite.fillRoundRect(cx - 11, cy + 20, 9, 16, 4, suit);
  _sprite.fillRoundRect(cx + 2 + (int)legSway, cy + 20, 9, 16, 4, suit);

  // Body / suit
  _sprite.fillRoundRect(cx - 13, cy - 8, 26, 28, 10, suit);
  // Chest panel with two small indicator lights
  _sprite.fillRoundRect(cx - 6, cy + 2, 12, 9, 2, CLR_ACCENT);
  _sprite.fillCircle(cx - 3, cy + 6, 1, CLR_RED);
  _sprite.fillCircle(cx + 3, cy + 6, 1, CLR_GREEN);

  // Helmet -- opaque dark visor, no eyes/mouth at all
  _sprite.fillCircle(cx, cy - 20, 15, suit);
  _sprite.fillCircle(cx + 2, cy - 20, 10, visor);
  _sprite.fillCircle(cx - 3, cy - 24, 2, CLR_ACCENT); // small reflection highlight

  // Lazy "z" drifting up and resetting, only part of the cycle so it
  // doesn't clutter the screen the whole time
  float zPhase = fmodf(_gaugeAnimT, 6.0f);
  if (zPhase < 4.0f)
  {
    int zy = cy - 40 - (int)(zPhase * 5);
    _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
    _sprite.setTextSize(1);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.drawString("z", cx + 20, zy);
    _sprite.setTextDatum(TL_DATUM);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Time-remaining visual — "rocket race"
//
// Replaces the old ring/arc dial completely. That design kept getting
// reported as having a visibility problem on the unfilled track segment even
// after two brightness bumps (0x2945 -> 0x6B6D -> 0x7BEF), and separately, a
// thin ring inscribed in a ~200x125px rectangular area leaves a lot of the
// area visually empty -- which is likely what read as "a lot of space
// between the task name and next task bar." This sidesteps both: there's no
// two-tone track/fill distinction to lose contrast on, and the countdown +
// track + decorations use most of the available width and height.
//
// A rocket flies from the left edge toward a flag on the right as the task
// progresses (position driven by `urgency`, the elapsed fraction) -- a
// kid-friendly, encouraging "getting closer" framing rather than a draining
// one. Everything is primitives (fillTriangle/fillRoundRect/fillCircle/
// drawFastHLine) since there's no icon font compiled in, same constraint as
// the weather icons and nav bar glyphs.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawTimeVisual(int cx, int cy, int halfW,
                                      float progressPct, int remainingSec, int totalSec,
                                      float urgency, uint32_t urgencyColor)
{
  (void)progressPct; // urgency (elapsed fraction) already carries this

  // ── Big countdown — the hero readout, color-coded same as always ─────────
  int remM = remainingSec / 60;
  int remS = remainingSec % 60;
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", remM, remS);
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextSize(3);
  _sprite.setTextColor(urgencyColor, CLR_BG);
  _sprite.drawString(timeBuf, cx, cy - 62);

  _sprite.setTextSize(1);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
  char ofBuf[20];
  snprintf(ofBuf, sizeof(ofBuf), "of %d min left", totalSec / 60);
  _sprite.drawString(ofBuf, cx, cy - 38);
  _sprite.setTextDatum(TL_DATUM);

  // ── A few twinkling stars in the "sky" above the track — pure whimsy ─────
  for (int i = 0; i < 4; i++)
  {
    int sx = cx - halfW + 15 + i * (2 * halfW - 30) / 3;
    int sy = cy - 20 + ((i % 2) ? 6 : -6);
    bool twinkle = (((int)_gaugeAnimT + i) % 3) == 0;
    _sprite.fillCircle(sx, sy, twinkle ? 2 : 1, CLR_YELLOW);
  }

  // ── Dashed track ──────────────────────────────────────────────────────────
  int leftX = cx - halfW, rightX = cx + halfW;
  for (int x = leftX; x < rightX - 6; x += 10)
    _sprite.drawFastHLine(x, cy, 6, CLR_SUBTEXT);

  // ── Finish flag — fixed at the right end, always visible as the goal ────
  _sprite.drawFastVLine(rightX, cy - 16, 22, CLR_SUBTEXT);
  _sprite.fillTriangle(rightX, cy - 16, rightX, cy - 8, rightX + 10, cy - 12, CLR_YELLOW);

  // ── Rocket — position driven by urgency: 0 = just started (left edge),
  // 1 = due now (arrived at the flag) ──────────────────────────────────────
  float u = constrain(urgency, 0.0f, 1.0f);
  int travel = (rightX - 14) - (leftX + 10); // keep clear of the start edge and the flag
  int rocketX = leftX + 10 + (int)(u * travel);
  int rocketY = cy;

  // Flame — trails behind (left, since travel is left-to-right), flickers
  int flameLen = 6 + (int)(sinf(_gaugeAnimT * 3.0f) * 2.0f);
  uint32_t flameColor = (((int)(_gaugeAnimT * 3.0f)) % 2 == 0) ? CLR_ORANGE : CLR_YELLOW;
  _sprite.fillTriangle(rocketX - 8, rocketY - 3, rocketX - 8, rocketY + 3,
                        rocketX - 8 - flameLen, rocketY, flameColor);

  // Fins
  _sprite.fillTriangle(rocketX - 6, rocketY - 4, rocketX - 1, rocketY - 4, rocketX - 6, rocketY - 9, CLR_ACCENT);
  _sprite.fillTriangle(rocketX - 6, rocketY + 4, rocketX - 1, rocketY + 4, rocketX - 6, rocketY + 9, CLR_ACCENT);

  // Body + window
  _sprite.fillRoundRect(rocketX - 6, rocketY - 4, 15, 8, 3, CLR_TEXT);
  _sprite.fillCircle(rocketX + 1, rocketY, 2, CLR_ACCENT);

  // Nose cone — colored by urgency, same signal the countdown text uses
  _sprite.fillTriangle(rocketX + 9, rocketY - 4, rocketX + 9, rocketY + 4, rocketX + 15, rocketY, urgencyColor);
}

void DisplayManager::setScreen(Screen s)
{
  _screen = s;
  if (!_spriteOk)
    return;
  _sprite.fillSprite(CLR_BG);
  _present();
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch polling
// Returns: -1=no touch, 0=main area, 1=All Tasks, 2=Mute,
//           3=Back (task list), 4=Weather row (clothing overlay),
//           5=Timer, 6=WiFi icon (IP toast)
// ─────────────────────────────────────────────────────────────────────────────
int DisplayManager::pollTouch()
{
  uint16_t tx, ty;
  if (!_getLogicalTouch(tx, ty))
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
  if (ty < STATUS_BAR_H && tx < 50)
    return 6; // WiFi icon → show IP toast
  if (ty >= STATUS_BAR_H && ty < STATUS_BAR_H + WEATHER_ROW_H)
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
// Clothing overlay — draws over the full screen, non-blocking (tickOverlay()
// dismisses it on tap or timeout).
// ─────────────────────────────────────────────────────────────────────────────
#include "ClothingAdvisor.h"
#include <audio/AudioManager.h>

void DisplayManager::showClothingOverlay()
{
  if (!weather.valid)
    return; // No data — nothing to recommend

  auto items = ClothingAdvisor::recommend(weather);
  _drawClothingOverlay(items);
  _present();

  _overlayKind = OverlayKind::CLOTHING;
  // No auto-timeout -- this already has explicit "tap to close"/"tap
  // anywhere to dismiss" affordances, and an 8s auto-dismiss was cutting it
  // off before it could actually be read (a full item list takes longer
  // than that to look through). Tap-only now; UINT32_MAX makes the
  // elapsed-time timeout check in tickOverlay() effectively never fire.
  _overlayDurationMs = UINT32_MAX;
  _overlayStart = millis();
}

// Draws a small popup ON TOP of whatever's already in the sprite (unlike the
// clothing overlay, which takes over the full screen) -- a toast, not a
// full-screen view -- then auto-dismisses after a few seconds via the same
// tickOverlay() path.
void DisplayManager::showIpToast()
{
  _drawIpToast();
  _present();

  _overlayKind = OverlayKind::IP_TOAST;
  _overlayDurationMs = 3000;
  _overlayStart = millis();
}

// Full-screen rocket-launch celebration, played once per task completion
// (see main.cpp's onTaskEvent, TaskEvent::COMPLETE). Unlike the other
// overlays, this one keeps redrawing itself for its whole duration --
// tickOverlay() below drives that -- rather than being drawn once on entry.
void DisplayManager::showTaskCompleteAnimation()
{
  _lastOverlayFrameMs = 0;
  _drawTaskCompleteAnimation(0.0f);
  _present();

  _overlayKind = OverlayKind::TASK_COMPLETE;
  _overlayDurationMs = 4000;
  _overlayStart = millis();
}

// Call this every loop() iteration. Non-blocking: checks for a tap or
// timeout, and if either fires, dismisses whichever overlay/toast is
// active and redraws the real screen underneath. TASK_COMPLETE additionally
// redraws a new animation frame (throttled to ~20fps, independent of the
// main 1-second tick) for as long as it stays active.
void DisplayManager::tickOverlay()
{
  if (_overlayKind == OverlayKind::NONE)
    return;

  uint32_t elapsed = millis() - _overlayStart;
  bool timedOut = elapsed >= _overlayDurationMs;

  if (_overlayKind == OverlayKind::TASK_COMPLETE && !timedOut)
  {
    if (millis() - _lastOverlayFrameMs > 50)
    {
      _lastOverlayFrameMs = millis();
      float t = (float)elapsed / (float)_overlayDurationMs;
      _drawTaskCompleteAnimation(t);
      _present();
    }
  }

  uint16_t tx, ty;
  bool tapped = _getLogicalTouch(tx, ty);

  if (tapped || timedOut)
  {
    _overlayKind = OverlayKind::NONE;

    time_t t = time(nullptr);
    std::tm *tm_now = localtime(&t);
    this->update(tm_now);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// IP address toast -- tap the WiFi icon in the status bar to see it. Shows
// on top of the current screen and auto-dismisses (see showIpToast() /
// tickOverlay()) rather than living on the main screen permanently.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawIpToast()
{
  String ip = WiFi.isConnected() ? WiFi.localIP().toString() : String("Not connected");

  const int w = 190, h = 56;
  const int x = (SPRITE_W - w) / 2;
  const int y = 100;

  _sprite.fillRoundRect(x, y, w, h, 8, CLR_CARD);
  _sprite.drawRoundRect(x, y, w, h, 8, CLR_ACCENT);

  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_CARD);
  _sprite.setTextSize(1);
  _sprite.drawString("Device IP", x + w / 2, y + 14);

  _sprite.setTextColor(CLR_TEXT, CLR_CARD);
  _sprite.setTextSize(2);
  _sprite.drawString(ip.c_str(), x + w / 2, y + 36);
  _sprite.setTextDatum(TL_DATUM);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task-complete celebration -- a big rocket climbs from the bottom of the
// screen to launched-off-the-top over the overlay's whole duration, using
// the full screen (not just the current-task card) for maximum drama, per
// request. t is 0..1 progress; called repeatedly by tickOverlay() with an
// increasing t rather than drawn once. Smoke puffs trail behind at the
// launch pad, the flame flickers, and the same starfield backdrop used
// elsewhere ties it into the rest of the space theme.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawTaskCompleteAnimation(float t)
{
  _sprite.fillRect(0, 0, SPRITE_W, SPRITE_H, CLR_BG);
  _drawStarfield(0, SPRITE_H);

  _sprite.setTextDatum(TC_DATUM);
  _sprite.setTextColor(CLR_GREEN, CLR_BG);
  _sprite.setTextSize(3);
  _sprite.drawString("Great job!", SPRITE_W / 2, 30);
  _sprite.setTextDatum(TL_DATUM);

  // Rocket climbs from near the bottom to well off the top of the screen
  const int startY = SPRITE_H - 70;
  const int endY = -60;
  int ry = startY + (int)((endY - startY) * t);
  int rx = SPRITE_W / 2 + (int)(sinf(t * 18.0f) * 4.0f); // slight wobble for character

  // Smoke puffs left behind near the launch pad, growing/fading as t advances
  for (int i = 0; i < 4; i++)
  {
    float puffT = t - i * 0.07f;
    if (puffT > 0.0f && puffT < 1.0f)
    {
      int py = startY + 26 + (int)(puffT * 50.0f);
      int px = SPRITE_W / 2 + ((i % 2 == 0) ? -1 : 1) * (int)(puffT * 26.0f);
      int prad = 4 + (int)(puffT * 10.0f);
      if (py < SPRITE_H + prad)
        _sprite.fillCircle(px, py, prad, CLR_SUBTEXT);
    }
  }

  // Flame -- flickers, trails below the rocket (opposite of travel = up)
  int flameLen = 14 + (int)(sinf(t * 36.0f) * 6.0f);
  uint32_t flameColor = (((int)(t * 20.0f)) % 2 == 0) ? CLR_ORANGE : CLR_YELLOW;
  _sprite.fillTriangle(rx - 8, ry + 20, rx + 8, ry + 20, rx, ry + 20 + flameLen, flameColor);

  // Fins
  _sprite.fillTriangle(rx - 12, ry + 14, rx - 4, ry + 14, rx - 12, ry + 24, CLR_ACCENT);
  _sprite.fillTriangle(rx + 12, ry + 14, rx + 4, ry + 14, rx + 12, ry + 24, CLR_ACCENT);

  // Body + window + nose cone
  _sprite.fillRoundRect(rx - 10, ry - 16, 20, 32, 8, CLR_TEXT);
  _sprite.fillCircle(rx, ry - 4, 5, CLR_ACCENT);
  _sprite.fillTriangle(rx - 10, ry - 16, rx + 10, ry - 16, rx, ry - 30, CLR_RED);
}

void DisplayManager::_drawTaskList()
{
  _sprite.fillRect(0, 0, 240, 320, CLR_BG);

  // Simple header with a back affordance (matches pollTouch's ty<30 zone)
  _sprite.fillRect(0, 0, 240, 30, CLR_STATUSBG);
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _sprite.setTextSize(2);
  _sprite.drawString("All Tasks", 120, 15);
  _sprite.setTextDatum(TL_DATUM);

  int y = 30 - _scrollOffset; // scroll shifts content up
  auto &sched = TaskScheduler::instance();

  for (size_t gi = 0; gi < sched.groups.size(); gi++)
  {
    auto &grp = sched.groups[gi];

    if (y > -24 && y < 320)
    { // only draw if roughly on-screen
      _sprite.fillRect(0, y, 240, 24, CLR_CARD);
      // Adaptive size (was a fixed size2 + hard truncate at 14 chars) so a
      // long group name shrinks to fit the ~195px available before the
      // +/- indicator at x=216, instead of getting clipped off-screen.
      int gSize = _fitTextSize(grp.name, 2, 195);
      _sprite.setTextColor(CLR_ACCENT, CLR_CARD);
      _sprite.setTextSize(gSize);
      _sprite.setCursor(10, y + (gSize == 2 ? 4 : 8));
      _sprite.print(grp.name.c_str());
      _sprite.setTextColor(CLR_TEXT, CLR_CARD);
      _sprite.setTextSize(2);
      _sprite.setCursor(216, y + 4);
      _sprite.print(_groupExpanded[gi] ? "-" : "+");
    }
    y += 24;

    if (!_groupExpanded[gi])
      continue;

    for (auto &task : grp.tasks)
    {
      if (y > -20 && y < 320)
      {
        _sprite.fillRect(0, y, 240, 20, CLR_BG);

        // Time and name are drawn separately (was one snprintf'd string at
        // a single size) so the name can size itself independently of the
        // fixed-width "H:MMx" time prefix, and so the time can stay 12-hour
        // without blowing out the combined string width.
        char timeBuf[10];
        format12hShort(task.startHour, task.startMin, timeBuf, sizeof(timeBuf));
        _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
        _sprite.setTextSize(1);
        _sprite.setCursor(20, y + 6);
        _sprite.print(timeBuf);

        int tSize = _fitTextSize(task.name, 2, 150);
        _sprite.setTextColor(CLR_TEXT, CLR_BG);
        _sprite.setTextSize(tSize);
        _sprite.setCursor(70, y + (tSize == 2 ? 2 : 6));
        _sprite.print(task.name.c_str());
      }
      y += 20;
    }
  }

  _contentHeight = y + _scrollOffset - 30; // total scrollable height
}

// ─────────────────────────────────────────────────────────────────────────────
// Clothing pictograms -- see the header comment above the declaration for
// why these exist (replacing the 2-letter text codes with actual shapes).
// Each one is built from the same primitive toolkit used everywhere else in
// this file (fillRect/fillTriangle/fillCircle/drawLine), roughly 20x20px,
// centered at (cx, cy). bgColor is the current row's background, used to
// cut small negative-space details rather than leaving solid blobs.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawClothingIcon(const String &code, int cx, int cy, uint32_t bgColor)
{
  if (code == "CT")
  {
    // Heavy coat -- wide body + sleeves + a V-neck cut at the collar
    _sprite.fillRoundRect(cx - 6, cy - 7, 12, 16, 2, CLR_ORANGE);
    _sprite.fillRect(cx - 10, cy - 5, 4, 10, CLR_ORANGE);
    _sprite.fillRect(cx + 6, cy - 5, 4, 10, CLR_ORANGE);
    _sprite.drawLine(cx - 3, cy - 7, cx, cy - 3, bgColor);
    _sprite.drawLine(cx + 3, cy - 7, cx, cy - 3, bgColor);
  }
  else if (code == "JK")
  {
    // Jacket -- slimmer/shorter than the coat, plus a center zipper line
    _sprite.fillRoundRect(cx - 5, cy - 6, 10, 13, 2, CLR_ORANGE);
    _sprite.fillRect(cx - 8, cy - 4, 3, 8, CLR_ORANGE);
    _sprite.fillRect(cx + 5, cy - 4, 3, 8, CLR_ORANGE);
    _sprite.drawFastVLine(cx, cy - 5, 11, bgColor);
  }
  else if (code == "TS")
  {
    // T-shirt -- body + two small angled sleeves + a shallow neckline
    _sprite.fillRect(cx - 6, cy - 3, 12, 12, CLR_PINK);
    _sprite.fillTriangle(cx - 6, cy - 3, cx - 10, cy + 2, cx - 6, cy + 3, CLR_PINK);
    _sprite.fillTriangle(cx + 6, cy - 3, cx + 10, cy + 2, cx + 6, cy + 3, CLR_PINK);
    _sprite.fillCircle(cx, cy - 3, 3, bgColor);
  }
  else if (code == "PT")
  {
    // Trousers -- waistband + two legs with a gap between them
    _sprite.fillRect(cx - 6, cy - 8, 12, 4, CLR_ACCENT);
    _sprite.fillRect(cx - 6, cy - 4, 5, 12, CLR_ACCENT);
    _sprite.fillRect(cx + 1, cy - 4, 5, 12, CLR_ACCENT);
  }
  else if (code == "SH")
  {
    // Shorts -- same as trousers but with short legs
    _sprite.fillRect(cx - 6, cy - 6, 12, 4, CLR_ACCENT);
    _sprite.fillRect(cx - 6, cy - 2, 5, 7, CLR_ACCENT);
    _sprite.fillRect(cx + 1, cy - 2, 5, 7, CLR_ACCENT);
  }
  else if (code == "BT")
  {
    // Boots -- vertical shaft + a wider foot with a heel notch
    _sprite.fillRect(cx - 3, cy - 8, 6, 10, CLR_SUBTEXT);
    _sprite.fillRoundRect(cx - 6, cy + 2, 13, 5, 2, CLR_SUBTEXT);
    _sprite.fillRect(cx - 6, cy + 2, 3, 3, bgColor);
  }
  else if (code == "SN")
  {
    // Trainers -- low rounded shoe body, sole line, small tongue
    _sprite.fillRoundRect(cx - 9, cy - 1, 18, 6, 3, CLR_YELLOW);
    _sprite.fillRect(cx + 3, cy - 5, 5, 5, CLR_YELLOW);
    _sprite.drawFastHLine(cx - 9, cy + 4, 18, bgColor);
  }
  else if (code == "UM")
  {
    // Umbrella -- dome canopy (circle, bottom half cut away) + handle
    _sprite.fillCircle(cx, cy - 2, 9, CLR_ACCENT);
    _sprite.fillRect(cx - 9, cy - 2, 18, 9, bgColor);
    _sprite.drawFastVLine(cx, cy - 2, 10, CLR_ACCENT);
    _sprite.drawLine(cx, cy + 8, cx - 3, cy + 8, CLR_ACCENT);
    _sprite.drawLine(cx - 3, cy + 8, cx - 3, cy + 6, CLR_ACCENT);
  }
  else if (code == "!!")
  {
    // Storm warning -- bold triangle with a cut-out exclamation mark
    _sprite.fillTriangle(cx, cy - 9, cx - 9, cy + 8, cx + 9, cy + 8, CLR_RED);
    _sprite.fillRect(cx - 1, cy - 3, 2, 6, bgColor);
    _sprite.fillCircle(cx, cy + 6, 1, bgColor);
  }
  else if (code == "SC")
  {
    // Scarf -- a wavy band with two hanging tails
    for (int i = -8; i < 8; i += 4)
      _sprite.drawLine(cx + i, cy - 2, cx + i + 2, cy + 2, CLR_GREEN);
    _sprite.fillRect(cx + 6, cy + 2, 3, 8, CLR_GREEN);
  }
  else if (code == "GL")
  {
    // Gloves/mittens -- rounded body + a thumb notch on the side
    _sprite.fillRoundRect(cx - 6, cy - 7, 11, 14, 4, CLR_GREEN);
    _sprite.fillRoundRect(cx - 9, cy - 2, 5, 6, 2, CLR_GREEN);
  }
  else if (code == "SG")
  {
    // Sunglasses -- two lenses + bridge + short arms
    _sprite.fillRoundRect(cx - 9, cy - 3, 7, 6, 2, CLR_TEXT);
    _sprite.fillRoundRect(cx + 2, cy - 3, 7, 6, 2, CLR_TEXT);
    _sprite.drawFastHLine(cx - 2, cy, 4, CLR_TEXT);
    _sprite.drawLine(cx - 9, cy - 1, cx - 12, cy - 3, CLR_TEXT);
    _sprite.drawLine(cx + 9, cy - 1, cx + 12, cy - 3, CLR_TEXT);
  }
  else if (code == "SS")
  {
    // Sunscreen -- small bottle with a cap
    _sprite.fillRoundRect(cx - 5, cy - 4, 10, 13, 2, CLR_YELLOW);
    _sprite.fillRect(cx - 3, cy - 8, 6, 4, CLR_YELLOW);
    _sprite.drawFastHLine(cx - 5, cy + 2, 10, bgColor);
  }
  else if (code == "HY")
  {
    // Hydration -- a water droplet
    _sprite.fillTriangle(cx, cy - 9, cx - 6, cy + 2, cx + 6, cy + 2, CLR_ACCENT);
    _sprite.fillCircle(cx, cy + 2, 6, CLR_ACCENT);
  }
  else
  {
    // Unmapped code -- small neutral square rather than nothing, so a
    // future new code doesn't silently disappear
    _sprite.fillRoundRect(cx - 6, cy - 6, 12, 12, 2, CLR_SUBTEXT);
  }
}

void DisplayManager::_drawClothingOverlay(const std::vector<ClothingItem> &items)
{
  _sprite.fillRect(0, 0, 240, 320, CLR_BG);

  // ── Header bar ───────────────────────────────────────────────────────────
  _sprite.fillRect(0, 0, 240, 28, CLR_STATUSBG);
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _sprite.setTextSize(1);
  _sprite.drawString("What to Wear", 100, 10);

  _sprite.setTextDatum(TR_DATUM);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _sprite.drawString("tap to close", 234, 10);

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
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
  _sprite.setTextSize(1);
  _sprite.drawString(summary, 120, 40);
  _sprite.setTextDatum(TL_DATUM);

  // ── Item list ────────────────────────────────────────────────────────────
  int y = 54;
  int maxItems = std::min((int)items.size(), 7);

  for (int i = 0; i < maxItems; i++)
  {
    uint32_t rowBg = (i % 2 == 0) ? CLR_CARD : CLR_BG;
    _sprite.fillRect(0, y, 240, 30, rowBg);
    _sprite.fillRect(0, y, 3, 30, CLR_ACCENT); // Left accent bar

    // Icon -- drawn pictogram instead of the raw 2-letter code as text
    _drawClothingIcon(items[i].icon, 20, y + 15, rowBg);

    // Label
    _sprite.setTextSize(1);
    _sprite.setCursor(36, y + 11);
    _sprite.print(items[i].label.substring(0, 26).c_str());

    y += 32;
  }

  // Overflow indicator
  if ((int)items.size() > maxItems)
  {
    _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
    _sprite.setTextDatum(MC_DATUM);
    _sprite.drawString(
        ("+" + String(items.size() - maxItems) + " more").c_str(),
        120, y + 6);
    _sprite.setTextDatum(TL_DATUM);
  }

  // ── Dismiss hint ─────────────────────────────────────────────────────────
  _sprite.fillRect(0, 300, 240, 20, CLR_STATUSBG);
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
  _sprite.drawString("Tap anywhere to dismiss", 120, 310);
  _sprite.setTextDatum(TL_DATUM);
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
  bool touched = _getLogicalTouch(tx, ty);

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
  _sprite.fillRect(0, 0, 240, 320, CLR_BG);
  _sprite.fillRect(0, 0, 240, 30, CLR_STATUSBG);
  _sprite.setTextDatum(MC_DATUM);
  _sprite.setTextColor(CLR_TEXT, CLR_STATUSBG);
  _sprite.drawString("Set Timer", 120, 15);

  const char *labels[6] = {"1 min", "5 min", "10 min", "15 min", "20 min", "30 min"};
  const int cols = 2, btnW = 104, btnH = 56, gapX = 8, gapY = 10;
  const int startX = (240 - (cols * btnW + gapX)) / 2, startY = 50;

  for (int i = 0; i < 6; i++)
  {
    int col = i % cols, row = i / cols;
    int x = startX + col * (btnW + gapX), y = startY + row * (btnH + gapY);
    _sprite.fillRoundRect(x, y, btnW, btnH, 8, CLR_CARD);
    _sprite.drawRoundRect(x, y, btnW, btnH, 8, CLR_SUBTEXT);
    _sprite.setTextColor(CLR_TEXT, CLR_CARD);
    _sprite.setTextSize(2);
    _sprite.drawString(labels[i], x + btnW / 2, y + btnH / 2);
  }
  _sprite.setTextSize(1);
  _sprite.setTextDatum(TL_DATUM);
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
  _sprite.fillRect(0, 0, 240, 320, CLR_BG);

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

  _sprite.setTextDatum(MC_DATUM);

  if (_timerDone)
  {
    _sprite.setTextColor(CLR_ACCENT, CLR_BG);
    _sprite.setTextSize(3);
    _sprite.drawString("Time's Up!", 120, 130);
    _sprite.setTextSize(1);
    _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
    _sprite.drawString("Tap below to dismiss", 120, 170);
  }
  else
  {
    uint32_t mm = remaining / 60, ss = remaining % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
    _sprite.setTextColor(CLR_TEXT, CLR_BG);
    _sprite.setTextSize(5);
    _sprite.drawString(buf, 120, 130);
    _sprite.setTextSize(1);

    float pct = 1.0f - (float)remaining / (float)_timerDurationSec;
    const int barW = 200, barH = 10, barX = 20, barY = 180;
    _sprite.drawRoundRect(barX, barY, barW, barH, 4, CLR_SUBTEXT);
    _sprite.fillRoundRect(barX + 1, barY + 1, (int)((barW - 2) * pct), barH - 2, 3, CLR_ACCENT);

    _sprite.setTextColor(CLR_SUBTEXT, CLR_BG);
    _sprite.drawString("Tap below to cancel", 120, 210);
  }

  _sprite.fillRoundRect(60, 260, 120, 40, 8, CLR_CARD);
  _sprite.drawRoundRect(60, 260, 120, 40, 8, CLR_RED);
  _sprite.setTextColor(CLR_RED, CLR_CARD);
  _sprite.drawString(_timerDone ? "OK" : "Cancel", 120, 280);
  _sprite.setTextDatum(TL_DATUM);
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
