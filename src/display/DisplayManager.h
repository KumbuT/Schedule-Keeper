#pragma once
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <vector>
#include "../tasks/TaskScheduler.h"
#include "Theme.h"   // shared CLR_* palette (moved out of this class)
#include "Widget.h"  // retained-mode widget framework + concrete widgets

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
  void showApSetupScreen(const String &ssid, const String &pass, const String &url); // First-boot Wi-Fi setup instructions
  void showClothingOverlay();       // Draws and arms the overlay, returns immediately
  void showIpToast();               // Draws a temporary "IP: x.x.x.x" toast, returns immediately
  void startTaskCelebration();      // small rocket orbits the task card for a few seconds on completion
  bool celebrating() const { return _celebrating; }
  void tickCelebration();           // call every loop iteration while celebrating() is true
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
  void toggleSchedPeek(); // expand/minimize the "scheduled task active" overlay on the manual timer

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
  void _drawCelebrationRocket(float t); // small orbiting rocket over the current-task card

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

  // ── Dirty-rectangle compositor state ──────────────────────────────────────
  // HOME and TASK_LIST render region-by-region: each top strip is redrawn and
  // pushed via _pushRegion() only when its own source data changed since the
  // last frame. These cache the last-drawn values so update() can detect a
  // change cheaply. _forceFullRedraw repaints every region once (set on every
  // setScreen(), and true at boot) so a screen switch always fully paints.
  bool _forceFullRedraw = true;
  StatusBarWidget _statusBar;       // top strip (clock/wifi/battery), widget model
  DelegateWidget _weatherRow;       // animated weather band -> _drawWeatherRow()
  DelegateWidget _currentTask;      // animated task card -> _drawCurrentTask()
  DelegateWidget _taskList;         // full-screen task list -> _drawTaskList()
  DelegateWidget _timerSet;         // full-screen timer presets -> _drawTimerSet()
  DelegateWidget _timerRunScreen;   // full-screen running timer -> _drawTimerRunning()
  NavBarWidget _navBar;             // HOME nav bar, migrated to the widget model
  char _dateStr[16] = "";               // "Wkd dd Mon" date, shown in the weather row (moved off the top bar)
  bool _schedPeek = false;              // scheduled-task overlay on the manual timer: false = minimized badge, true = expanded card
  bool     _celebrating = false;        // task-complete rocket flourish active
  uint32_t _celebrateStart = 0;         // millis the celebration began
  uint32_t _celebrateLastFrame = 0;     // throttle for the celebration redraw
  bool _overlayAwaitingRelease = false; // ignore the still-held opening tap until the finger lifts
  int _ovDebounce = 0;                  // consecutive-poll counter for debounced overlay dismiss

  void _present();                                        // push the finished sprite frame to the real panel

  // Rotation-aware partial-push primitive: copies one LOGICAL (240x320) sprite
  // rectangle onto the real (320x240) panel, applying the same 90/270 rotation
  // pushRotated() would, but for that sub-region only. This is the single place
  // the rotation is handled for the dirty-rectangle renderer. Pushed row-by-row
  // via pushImage() from a small stack line buffer (no full-frame allocation).
  void _pushRegion(int lx, int ly, int w, int h);
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
  // True right after entering TASK_LIST until the touch that opened it (if
  // still physically down at that instant) is released. Without this,
  // updateTaskListScroll() would treat that still-down finger as a brand
  // new tap gesture the moment the screen switches, and toggle whatever
  // group happens to sit under the "All Tasks" nav button's coordinates --
  // a ghost tap the user never actually made on this screen. See setScreen()
  // and updateTaskListScroll().
  bool _taskListAwaitingRelease = false;
  bool _groupExpanded[16] = {};
  float _gaugeAnimT = 0.0f; // Seconds elapsed — drives gauge bob and arc pulse

  // Drawing methods (all draw onto _sprite). Status bar migrated to
  // StatusBarWidget (see Widget.h).
  void _drawWeatherRow();
  void _drawCurrentTask();
  // Compositor helper: if `full` force the widget dirty; then, if it is dirty
  // and visible, draw it into the sprite and push just its bounds. Clears the
  // dirty flag. The generic path all migrated widgets go through.
  void _composite(Widget &w, bool full);
  void _drawTaskList();
  void _drawClothingOverlay(const std::vector<struct ClothingItem> &items);
  void _handleTaskListTap(int ty);

  // Draws one small clothing pictogram centered at (cx,cy), sized to fit an
  // roughly `size`x`size` box, for a single ClothingAdvisor code (CT/JK/TS/
  // PT/SH/BT/SN/UM/!!/SC/GL/SG/SS/HY). Primitives-only, same approach as the
  // weather icons. Used by _drawClothingOverlay to render a list of
  // icon+label rows (one row per recommended item).
  void _drawClothingIcon(const String &code, int cx, int cy, int size);

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

  // Small floating companion for the astronaut scene -- a cat in a tiny
  // round helmet with its ears poking out above the rim. Deliberately no
  // face at all (same opaque-visor trick as the astronaut) since the
  // earlier stand-alone cat's face/ear proportions were the likely reason
  // it read as "hideous"; here it's a small secondary character, not the
  // focal point, and animates on its own independent phase so it doesn't
  // move in lockstep with the astronaut.
  void _drawAstroCat(int cx, int cy);

  // Scattered small twinkling dots across a vertical band -- shared
  // backdrop for the active-task rocket-race view, the idle astronaut
  // scene, and the task-complete launch animation, so the whole card reads
  // as one consistent space theme rather than three unrelated screens.
  // Each star's on/off phase is offset by its own index so they don't all
  // blink in sync.
  void _drawStarfield(int top, int bottom);

  // Widget helpers (wifi + battery moved into StatusBarWidget, see Widget.h)
  void _drawProgressBar(int x, int y, int w, int h, float pct, uint32_t color);
  void _drawTimerSet();
  void _drawTimerRunning();
  // Rainbow visual-timer dial variant (Time Timer style): fixed 60-minute face
  // with a concentric rainbow wedge that depletes clockwise. Selected via
  // Config.timerStyle == 1; branched from _drawTimerRunning().
  void _drawTimerRainbowDial(int cx, int cy, int R, uint32_t remainingSec,
                             uint32_t totalSec, bool done, bool compact);
  // "Scheduled task is running" overlay shown on the manual timer screen:
  // a minimized corner badge, or an expanded status card. _schedPeek is the
  // expanded/minimized state (default minimized).
  void _drawSchedPeek();
  // Reward/streak visuals. _drawStar draws a small 5-point star (filled = earned).
  // _drawRewardPanel shows today's stars + streak on the idle home scene.
  void _drawStar(int cx, int cy, int r, bool filled, uint32_t color);
  void _drawRewardPanel(int cyTop);
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
  String _truncateToFit(const String &s, int size, int maxWidthPx);

  uint32_t _timerDurationSec = 0;
  uint32_t _timerStartMillis = 0;
  bool _timerRunning = false;
  bool _timerDone = false;

  // Color palette (CLR_*) now lives in Theme.h so the Widget classes share it.
};
