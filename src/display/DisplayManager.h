#pragma once
#include <TFT_eSPI.h>
#include <Preferences.h>
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
  void showClothingOverlay();       // Draws and arms the overlay, returns immediately
  void showIpToast();               // Draws a temporary "IP: x.x.x.x" toast, returns immediately
  void showTaskCompleteAnimation(); // Full-screen rocket-launch celebration, ~4s, plays once per task completion
  void tickOverlay();                // Call every loop() iteration while any overlay/toast is active
  bool overlayActive() const { return _overlayKind != OverlayKind::NONE; }
  bool consumeDirty()
  {
    bool d = _screenDirty;
    _screenDirty = false;
    return d;
  }

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute, 3=back, 4=weather, 6=wifi icon
  int pollTouch();
  void startTimer(uint32_t seconds);

private:
  int _touchStartY = 0;
  bool _isDragging = false;

  // Generalized so the clothing overlay and the IP toast (different content,
  // different auto-dismiss durations) can share one timeout/dismiss code
  // path instead of duplicating it.
  enum class OverlayKind
  {
    NONE,
    CLOTHING,
    IP_TOAST,
    TASK_COMPLETE
  };
  OverlayKind _overlayKind = OverlayKind::NONE;
  uint32_t _overlayStart = 0;
  uint32_t _overlayDurationMs = 0;
  uint32_t _lastOverlayFrameMs = 0; // throttles TASK_COMPLETE's redraw rate independent of the main 1s tick
  void _drawIpToast();

  // Full-screen "rocket launch" celebration played once when a scheduled
  // task completes (see main.cpp's onTaskEvent TaskEvent::COMPLETE). Unlike
  // the other overlays this one actually animates continuously over its
  // whole duration (tickOverlay() redraws it every ~50ms), not just once on
  // entry -- t is 0..1 progress through _overlayDurationMs, driving a
  // rocket that climbs from the bottom of the screen to launched-off-the-
  // top, using the full screen height for maximum drama.
  void _drawTaskCompleteAnimation(float t);

  DisplayManager() : _sprite(&_tft) {}

  // ───────────────────────────────────────────────────────────────────────
  // HARDWARE-ROTATION SHIM
  //
  // This panel's controller only addresses correctly at TFT_eSPI rotation 1
  // (320 wide x 240 tall) -- rotations 0/2 ("240x320 portrait") come out
  // transposed on this specific unit, confirmed on both ILI9341_DRIVER and
  // ILI9341_2_DRIVER. Rather than rewrite every pixel coordinate in this
  // file for a 320x240 layout, we keep drawing into a virtual 240x320
  // canvas (_sprite) exactly as before, then hardware-rotate the whole
  // finished frame onto the real 320x240 panel with pushRotated(). The
  // physical display module gets mounted rotated 90 degrees in the case so
  // the rotated image reads right-side-up.
  //
  // _tft   : the real hardware object. Only used for init/rotation and for
  //          raw touch reads (_tft.getTouch()) -- never drawn on directly.
  // _sprite: an off-screen 240x320 canvas. All drawing happens here, using
  //          the exact same coordinates this file always used.
  //
  // TWO CONSTANTS BELOW ARE THE ONLY THINGS THAT MAY NEED FLIPPING once
  // this is on real hardware, since the exact rotation direction can't be
  // verified without it:
  //   SPRITE_ROTATE_DEG   -- try 90 first; if the image on the panel is
  //                          mirrored/upside-down relative to how you
  //                          physically mounted the glass, change to 270.
  //   (touch inverse math is derived from the same constant, in the .cpp,
  //    so it stays consistent automatically if you flip this.)
  // ───────────────────────────────────────────────────────────────────────
  static constexpr int SPRITE_W = 240;             // logical canvas width  (unchanged from original design)
  static constexpr int SPRITE_H = 320;             // logical canvas height (unchanged from original design)
  static constexpr int SPRITE_ROTATE_DEG = 90;      // <-- flip to 270 if orientation comes out wrong

  // Layout bands within the 240x320 logical canvas, top to bottom:
  // status bar -> weather row -> current-task card (+ next-task strip) ->
  // nav bar. STATUS_BAR_H grew from the original 20px to fit a bigger clock;
  // everything below it shifted down by the same amount so nothing overlaps.
  static constexpr int STATUS_BAR_H = 28;
  static constexpr int WEATHER_ROW_H = 38;

  TFT_eSPI _tft;
  TFT_eSprite _sprite;
  bool _spriteOk = false;

  void _present();                                        // push the finished sprite frame to the real panel
  bool _getLogicalTouch(uint16_t &lx, uint16_t &ly);       // physical getTouch() -> logical (sprite-space) coords

  // ───────────────────────────────────────────────────────────────────────
  // Touch calibration -- NO LONGER done inline in this firmware at all.
  // Calibration now lives entirely in a separate, standalone PlatformIO
  // project: ../touch_calibration_tool. That tool uses TFT_eSPI's own
  // tft.calibrateTouch() (its native calibration UI) and writes the
  // resulting 5-value array to NVS under the same "touchcal"/"cal"
  // namespace/key this firmware reads. Flash that tool once, run it, tap
  // its prompts, then reflash this firmware -- it just loads whatever's in
  // NVS below.
  //
  // Why calibration moved out entirely instead of staying inline (even in
  // a safer form): tft.calibrateTouch() contains a bare
  // `while (!validTouch(...));` per sample point with no timeout at all,
  // and this panel's touch is confirmed to be genuinely marginal (needed
  // SPI_TOUCH_FREQUENCY lowered, needs firmer-than-usual pressure at
  // corners/edges). That's an acceptable, watchable risk in a one-off
  // manual utility you run once and can power-cycle if it hangs -- it is
  // NOT acceptable inline in this firmware's boot path, where a single bad
  // sample would hang the device every time it powers on. Keeping the two
  // concerns in separate binaries gets the library's own calibration UI
  // without that boot-time risk.
  // ───────────────────────────────────────────────────────────────────────
  uint16_t _touchCal[5] = {0, 0, 0, 0, 0};
  void _initTouchCalibration(); // loads _touchCal from NVS only -- never calibrates itself

  // ───────────────────────────────────────────────────────────────────────
  // Ongoing (post-calibration) touch reads -- NOT via _tft.getTouch().
  // getTouch() runs the raw XPT2046 samples through TFT_eSPI's own
  // validTouch(), a strict "two consecutive raw samples within 20 units,
  // default Z threshold 350" debounce this panel's marginal touch line
  // can't reliably satisfy. This reads raw X/Y/Z directly with a looser
  // threshold/acceptance (single re-check, no exact-match requirement),
  // then applies our own copy of TFT_eSPI's calibration math (using the
  // stored _touchCal array, populated from NVS by _initTouchCalibration())
  // instead of handing off to getTouch() at all.
  // ───────────────────────────────────────────────────────────────────────
  bool _getRuntimeTouch(uint16_t &screenX, uint16_t &screenY);

  Screen _screen = Screen::HOME;
  bool _screenDirty = true;
  int _scrollOffset = 0;   // For task list scrolling
  int _contentHeight = 0;  // total height of task list content (set by _drawTaskList)
  bool _touchDown = false; // true while finger is held down on TASK_LIST screen
  int _lastTouchY = 0;     // previous frame's touch Y, for delta calculation
  bool _groupExpanded[16] = {};
  float _gaugeAnimT = 0.0f; // Seconds elapsed — drives gauge bob and arc pulse

  // Drawing methods (all draw onto _sprite)
  void _drawStatusBar(struct tm *now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();
  void _drawClothingOverlay(const std::vector<struct ClothingItem> &items);
  void _handleTaskListTap(int ty);

  // Small pictogram for each ClothingAdvisor code (CT/JK/TS/PT/SH/BT/SN/UM/
  // !!/SC/GL/SG/SS/HY) -- replaces printing the raw 2-letter code as text.
  // Those codes existed only because real emoji have no glyphs in the
  // compiled bitmap fonts; drawing small shapes from primitives (same
  // approach as the weather icons and nav bar glyphs) gets an actual
  // picture instead of a letter abbreviation, with no font/asset loading
  // involved. cx/cy is the icon's center; roughly a 20x20px footprint.
  // Each icon picks its own thematic accent color internally rather than
  // using a single passed-in color, so the list reads as colorful at a
  // glance rather than a column of same-colored silhouettes; bgColor is the
  // row's current background (alternating CLR_CARD/CLR_BG) and is used to
  // cut negative-space details -- V-necks, a thumb notch, the exclamation
  // mark, a shoe sole line -- so shapes don't read as solid blobs.
  void _drawClothingIcon(const String &code, int cx, int cy, uint32_t bgColor);

  // Time-remaining visual — replaces the old ring/arc dial entirely. The
  // ring kept getting reported as having a "background issue" (the unfilled
  // track segment not reading as visible) even after two contrast bumps on
  // CLR_GAUGE_BG, and it left a lot of dead space in the card since a thin
  // circle doesn't fill a roughly 200x125px rectangular area well. This is
  // a kid-friendly "rocket race" instead: a big countdown number, then a
  // rocket that flies left-to-right along a dashed track toward a finish
  // flag as the task progresses. No thin two-tone ring anywhere, so the
  // old contrast problem has no equivalent to reproduce.
  // cx/cy = center of the whole visual, halfW = half-width of the track
  void _drawTimeVisual(int cx, int cy, int halfW,
                        float progressPct, int remainingSec, int totalSec,
                        float urgency, uint32_t urgencyColor);

  // Idle animation for the "no active task" state -- previously static
  // gray text, then a small cat (deemed "hideous" -- likely the small
  // hand-drawn face/ear proportions reading as uncanny at this pixel
  // budget). Replaced with a space theme to match the rocket-race timer:
  // a sleepy astronaut floating in zero-g, no face details at all (just an
  // opaque dark visor, sidestepping the same uncanny-face risk), gently
  // bobbing/swaying limbs, and a drifting "Zzz".
  void _drawSleepyAstronaut(int cx, int cy);

  // Scattered small twinkling dots across a vertical band -- shared
  // backdrop for the active-task rocket-race view, the idle astronaut
  // scene, and the task-complete launch animation, so the whole card reads
  // as one consistent space theme rather than three unrelated screens.
  // Each star's on/off phase is offset by its own index so they don't all
  // blink in sync.
  void _drawStarfield(int top, int bottom);

  // Widget helpers
  void _drawWifiArcs(int cx, int cy, int rssi);
  void _drawBatteryIcon(int x, int y, int pct);
  void _drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color);
  void _drawTimerSet();
  void _drawTimerRunning();
  int _handleTimerSetTouch(uint16_t tx, uint16_t ty);
  int _handleTimerRunningTouch(uint16_t tx, uint16_t ty);

  // Animated weather glyph (sun/cloud/rain/snow/storm/fog + a small wind
  // badge) drawn from primitives -- replaces the old description/humidity/
  // wind text, which is dropped per request ("images convey the message
  // better than words"). Category comes from weather.icon (OpenWeatherMap
  // code); animation phase comes from _gaugeAnimT, which now actually
  // advances (see update()) instead of sitting frozen at 0.
  void _drawWeatherIcon(int cx, int cy);

  // Returns the largest text size in [1, maxSize] whose rendered width
  // (default GLCD font: 6px/char per size step) fits within maxWidthPx --
  // used so long task names shrink instead of clipping.
  int _fitTextSize(const String &s, int maxSize, int maxWidthPx);

  uint32_t _timerDurationSec = 0;
  uint32_t _timerStartMillis = 0;
  bool _timerRunning = false;
  bool _timerDone = false;

  // Color palette (RGB565)
  static constexpr uint32_t CLR_BG = 0x1082;
  static constexpr uint32_t CLR_CARD = 0x2965; // was 0x2104 -- a bit more saturated for a livelier feel
  static constexpr uint32_t CLR_ACCENT = 0x07FF; // cyan
  static constexpr uint32_t CLR_TEXT = 0xFFFF;
  static constexpr uint32_t CLR_SUBTEXT = 0xAD75; // gray
  static constexpr uint32_t CLR_GREEN = 0x07E0;
  static constexpr uint32_t CLR_YELLOW = 0xFFE0;
  static constexpr uint32_t CLR_RED = 0xF800;
  static constexpr uint32_t CLR_ORANGE = 0xFDA0;
  static constexpr uint32_t CLR_PINK = 0xFE19;
  static constexpr uint32_t CLR_BAR_BG = 0x39C7;
  static constexpr uint32_t CLR_STATUSBG = 0x0841;
  static constexpr uint32_t CLR_CLOUD = 0xC618; // weather-icon cloud body (TFT_SILVER)
};
