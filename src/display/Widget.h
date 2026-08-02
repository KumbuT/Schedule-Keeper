#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <functional>
#include "Theme.h"

// ─────────────────────────────────────────────────────────────────────────────
// Retained-mode UI framework for partial (dirty-rectangle) rendering.
//
// Everything here works in LOGICAL sprite space: 240 (w) x 320 (h), the same
// coordinate space every existing draw call already uses. The 90/270 rotation
// onto the real 320x240 panel is handled in exactly ONE place -- the
// compositor's _pushRegion() primitive in DisplayManager -- so nothing in a
// Widget ever needs to think about rotation.
//
// Contract:
//   * A Widget owns a bounding Rect and a `dirty` flag.
//   * It marks itself dirty ONLY when its own model data changes (that
//     comparison lives in the widget's setters). Unchanged => not redrawn,
//     not pushed.
//   * Each frame the compositor: (1) redraws every dirty+visible widget into
//     the shared canvas, clipped to its bounds; (2) pushes each dirty bound to
//     the panel via _pushRegion(); (3) clears the dirty flags.
// ─────────────────────────────────────────────────────────────────────────────

// Axis-aligned rectangle in logical (240x320) sprite space.
struct Rect
{
  int16_t x, y, w, h;

  // Explicit constructors (not default-member-initializers) so this stays
  // brace-constructible as `Rect{x,y,w,h}` under C++11, which the Arduino-ESP32
  // toolchain compiles with -- default member initializers make a struct a
  // non-aggregate in C++11 and break `Rect{...}`. Params are int to avoid
  // narrowing warnings at the call sites.
  Rect() : x(0), y(0), w(0), h(0) {}
  Rect(int x_, int y_, int w_, int h_)
      : x((int16_t)x_), y((int16_t)y_), w((int16_t)w_), h((int16_t)h_) {}

  bool valid() const { return w > 0 && h > 0; }
  int16_t right() const { return (int16_t)(x + w); }
  int16_t bottom() const { return (int16_t)(y + h); }

  bool intersects(const Rect &o) const
  {
    return x < o.right() && right() > o.x && y < o.bottom() && bottom() > o.y;
  }

  bool contains(int16_t px, int16_t py) const
  {
    return px >= x && px < right() && py >= y && py < bottom();
  }

  // Smallest rectangle covering both a and b (ignores invalid operands).
  static Rect merge(const Rect &a, const Rect &b)
  {
    if (!a.valid()) return b;
    if (!b.valid()) return a;
    int16_t x0 = min(a.x, b.x), y0 = min(a.y, b.y);
    int16_t x1 = max(a.right(), b.right()), y1 = max(a.bottom(), b.bottom());
    return Rect{x0, y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
  }
};

// Base class for every on-screen element.
class Widget
{
public:
  explicit Widget(const Rect &bounds) : _bounds(bounds) {}
  virtual ~Widget() {}

  const Rect &bounds() const { return _bounds; }
  bool isDirty() const { return _dirty; }
  bool isVisible() const { return _visible; }

  void markDirty() { _dirty = true; }
  void clearDirty() { _dirty = false; }

  void setVisible(bool v)
  {
    if (v != _visible)
    {
      _visible = v;
      _dirty = true; // appearing or disappearing both require a repaint
    }
  }

  // Animated widgets return true so the compositor repaints them every frame
  // regardless of the dirty flag (change-detection can't capture animation).
  virtual bool alwaysRepaint() const { return false; }

  // Paint self into `canvas`. The compositor has already clipped the canvas
  // viewport to _bounds, so any drawing that strays outside is safely clipped
  // -- but a well-behaved widget paints its own background first so it fully
  // owns every pixel inside its bounds (no stale content bleeds through).
  virtual void draw(TFT_eSprite &canvas) = 0;

  // Optional tap handling at logical (lx,ly). Return true if consumed.
  virtual bool onTap(int16_t lx, int16_t ly)
  {
    (void)lx;
    (void)ly;
    return false;
  }

protected:
  void setBounds(const Rect &b) { _bounds = b; }

  Rect _bounds;
  bool _dirty = true; // paint once on first composite
  bool _visible = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// Bottom navigation bar (HOME screen): Tasks | Timer | Mute, drawn from
// primitives. First element migrated to the widget model. Owns only its mute
// state; the compositor draws it and pushes its bounds when dirty. Tapping is
// still routed by the existing pollTouch() zones in DisplayManager for now.
// ─────────────────────────────────────────────────────────────────────────────
class NavBarWidget : public Widget
{
public:
  NavBarWidget() : Widget(Rect{0, 268, 240, 52}) {}

  // Change-detecting setter: only marks dirty when the mute state flips.
  void setMuted(bool m)
  {
    if (m != _muted)
    {
      _muted = m;
      markDirty();
    }
  }

  // Enable/disable the Timer button. Disabled while a scheduled task is running
  // (the manual timer isn't allowed then). Drawn dimmed when disabled.
  void setTimerEnabled(bool e)
  {
    if (e != _timerEnabled)
    {
      _timerEnabled = e;
      markDirty();
    }
  }
  bool timerEnabled() const { return _timerEnabled; }

  void draw(TFT_eSprite &c) override
  {
    const int y = _bounds.y, h = _bounds.h;
    const int midY = y + h / 2;

    c.fillRect(0, y, 240, h, CLR_STATUSBG);
    c.drawFastHLine(0, y, 240, CLR_SUBTEXT);
    c.drawFastVLine(80, y, h, CLR_SUBTEXT);
    c.drawFastVLine(160, y, h, CLR_SUBTEXT);

    // Tasks: a small 3-row checklist (green)
    {
      const int cx = 40;
      for (int row = -1; row <= 1; row++)
      {
        int ry = midY + row * 7;
        c.drawRect(cx - 13, ry - 2, 5, 5, CLR_GREEN);
        c.drawFastHLine(cx - 5, ry, 13, CLR_GREEN);
      }
    }

    // Timer: a clock face with hands and a top stem. Dimmed to gray when the
    // button is disabled (a scheduled task is running).
    {
      const int cx = 120;
      uint32_t tc = _timerEnabled ? CLR_ORANGE : CLR_SUBTEXT;
      c.drawCircle(cx, midY, 10, tc);
      c.fillRect(cx - 1, midY - 13, 3, 3, tc);
      c.drawLine(cx, midY, cx, midY - 6, tc);
      c.drawLine(cx, midY, cx + 5, midY, tc);
    }

    // Mute: speaker cone, plus sound-wave chevrons or a slash when muted
    {
      const int cx = 200;
      uint32_t color = _muted ? CLR_YELLOW : CLR_ACCENT;
      c.fillRect(cx - 14, midY - 3, 4, 6, color);
      c.fillTriangle(cx - 10, midY - 3, cx - 10, midY + 3, cx - 2, midY + 8, color);
      c.fillTriangle(cx - 10, midY - 3, cx - 2, midY + 8, cx - 2, midY - 8, color);
      if (_muted)
      {
        c.drawLine(cx - 14, midY - 10, cx + 6, midY + 10, CLR_RED);
        c.drawLine(cx - 14, midY + 10, cx + 6, midY - 10, CLR_RED);
      }
      else
      {
        c.drawLine(cx + 2, midY - 6, cx + 8, midY, color);
        c.drawLine(cx + 8, midY, cx + 2, midY + 6, color);
        c.drawLine(cx + 6, midY - 10, cx + 14, midY, color);
        c.drawLine(cx + 14, midY, cx + 6, midY + 10, color);
      }
    }
  }

private:
  bool _muted = false;
  bool _timerEnabled = true;
};

// ─────────────────────────────────────────────────────────────────────────────
// Status bar (top strip): wifi signal bars (left), 12-hour clock + date
// (center), battery gauge (right). Owns its model (clock, date, battery %,
// wifi RSSI) and only marks dirty when a displayed value actually changes.
// Height must match DisplayManager::STATUS_BAR_H (28).
// ─────────────────────────────────────────────────────────────────────────────
class StatusBarWidget : public Widget
{
public:
  StatusBarWidget() : Widget(Rect{0, 0, 240, 28}) {}

  void setClock(int hour24, int minute)
  {
    if (hour24 != _hour || minute != _min)
    {
      _hour = hour24;
      _min = minute;
      markDirty();
    }
  }
  void setTz(const char *z)
  {
    if (_tz != z)
    {
      _tz = z;
      markDirty();
    }
  }
  void setBattery(int pct)
  {
    if (pct != _batt)
    {
      _batt = pct;
      markDirty();
    }
  }
  void setWifi(int rssi)
  {
    int bars = _bucket(rssi);
    if (bars != _bars)
      markDirty();
    _rssi = rssi;
    _bars = bars;
  }

  void draw(TFT_eSprite &c) override
  {
    c.fillRect(0, 0, 240, 28, CLR_STATUSBG);
    _drawWifi(c, 14, 10);
    _drawBattery(c, 184, 7);

    // Clock -- 12-hour with AM/PM, computed by hand (strftime %l is unreliable
    // on this toolchain, %I zero-pads).
    int h12 = _hour % 12;
    if (h12 == 0)
      h12 = 12;
    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", h12, _min,
             _hour < 12 ? "AM" : "PM");
    // Clock + timezone on ONE line, centered as a unit between the wifi bars
    // (left) and the battery (right). The time is the hero (size 2); the tz
    // abbreviation sits just to its right, subdued, at size 1.
    c.setTextSize(2);
    int tw = c.textWidth(timeBuf);
    c.setTextSize(1);
    int zw = _tz.length() ? c.textWidth(_tz.c_str()) : 0;
    const int gap = 6;
    int total = tw + (zw ? gap + zw : 0);
    int startX = 120 - total / 2;
    if (startX < 30) startX = 30; // stay clear of the wifi bars

    c.setTextDatum(ML_DATUM); // left-anchored, vertically centered on the strip
    c.setTextColor(CLR_TEXT, CLR_STATUSBG);
    c.setTextSize(2);
    c.drawString(timeBuf, startX, 14);

    if (zw)
    {
      c.setTextColor(CLR_SUBTEXT, CLR_STATUSBG);
      c.setTextSize(1);
      c.drawString(_tz.c_str(), startX + tw + gap, 14);
    }
    c.setTextDatum(TL_DATUM);
  }

private:
  static int _bucket(int rssi)
  {
    if (rssi == 0) return 0; // disconnected
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    return 1;
  }

  void _drawWifi(TFT_eSprite &c, int cx, int cy)
  {
    if (_rssi == 0) // disconnected -- a small red "x"
    {
      c.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, CLR_RED);
      c.drawLine(cx + 5, cy - 5, cx - 5, cy + 5, CLR_RED);
      return;
    }
    const int barW = 3, gap = 2;
    const int heights[4] = {4, 7, 10, 13};
    const int baseline = cy + 7;
    const int totalW = 4 * barW + 3 * gap;
    int x = cx - totalW / 2;
    for (int i = 0; i < 4; i++)
    {
      uint32_t color = (i < _bars) ? CLR_ACCENT : CLR_SUBTEXT;
      c.fillRect(x, baseline - heights[i], barW, heights[i], color);
      x += barW + gap;
    }
  }

  void _drawBattery(TFT_eSprite &c, int x, int y)
  {
    uint32_t fillColor = _batt > 50 ? CLR_GREEN : (_batt > 20 ? CLR_YELLOW : CLR_RED);
    c.drawRect(x, y, 22, 10, CLR_TEXT);
    c.fillRect(x + 22, y + 3, 2, 4, CLR_TEXT); // nub
    int fillW = (int)(_batt / 100.0f * 20);
    if (fillW < 1)
      fillW = 1;
    c.fillRect(x + 1, y + 1, fillW, 8, fillColor);

    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", _batt);
    c.setTextColor(CLR_TEXT, CLR_STATUSBG);
    c.setTextSize(1);
    c.setTextDatum(TR_DATUM);
    c.drawString(buf, 236, y + 1);
    c.setTextDatum(TL_DATUM);
  }

  int _hour = -1, _min = -1;
  String _tz;
  int _batt = -1;
  int _rssi = 0, _bars = -1;
};

// ─────────────────────────────────────────────────────────────────────────────
// A widget whose painting is delegated to a callback (typically a
// DisplayManager method). Used for the large *animated* elements -- the weather
// row and the current-task card -- whose substantial draw code stays in
// DisplayManager rather than being relocated into a header. They still live in
// the compositor and push only their own region like every other widget.
// Constructed empty; DisplayManager::begin() calls configure() with the bounds,
// the paint callback, and whether it animates (alwaysRepaint => redraw/push
// every tick).
// ─────────────────────────────────────────────────────────────────────────────
class DelegateWidget : public Widget
{
public:
  using PaintFn = std::function<void(TFT_eSprite &)>;

  DelegateWidget() : Widget(Rect{}) {}

  void configure(const Rect &b, PaintFn fn, bool always)
  {
    setBounds(b);
    _fn = fn;
    _always = always;
    markDirty();
  }

  bool alwaysRepaint() const override { return _always; }
  void draw(TFT_eSprite &c) override
  {
    if (_fn)
      _fn(c);
  }

private:
  PaintFn _fn;
  bool _always = false;
};
