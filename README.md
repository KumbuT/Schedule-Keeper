# Schedule Tracker

A small, kid-friendly desk device that shows the day's schedule, runs task
timers, plays spoken prompts, and suggests what to wear based on the weather.
It runs on a **Seeed Studio XIAO ESP32-C3** driving a **2.8" ILI9341 TFT with an
XPT2046 resistive touch panel**, and is configured over WiFi from any browser.

The firmware is written in C++ on the Arduino framework and built with
PlatformIO.

---

## Table of contents

- [What it does](#what-it-does)
- [Hardware](#hardware)
- [Wiring](#wiring)
- [Repository layout](#repository-layout)
- [Software architecture](#software-architecture)
- [Build & flash](#build--flash)
- [First-time setup (WiFi & config)](#first-time-setup-wifi--config)
- [Touch calibration](#touch-calibration)
- [Voice prompts (audio)](#voice-prompts-audio)
- [Schedule format](#schedule-format)
- [Web dashboard & API](#web-dashboard--api)
- [Memory notes](#memory-notes)
- [Troubleshooting](#troubleshooting)

---

## What it does

The device always shows a **home screen** with a status bar (12-hour clock,
date, WiFi signal, battery), a **weather row**, and a **current-task card**.
When a scheduled task is active it shows a playful "rocket race" countdown — a
rocket travels toward a finish flag as time runs out; when no task is active it
shows an idle sleepy-astronaut animation over a twinkling starfield.

From the home screen you can:

- Open an **All Tasks** list (grouped, collapsible, scrollable).
- Start a **manual timer** from preset durations.
- **Mute / unmute** the spoken prompts.
- Tap the **weather row** for a full-screen **"What to Wear"** recommendation
  (icons + labels derived from temperature, wind, humidity, and conditions).
- Tap the **WiFi icon** to show the device's IP address as a toast.

Scheduled tasks fire spoken audio prompts at start, halfway, one-minute-left,
and completion, and completion triggers a full-screen rocket-launch celebration.

Everything — WiFi, timezone, city/weather API key, units, and the schedule
itself — is configured from a browser; nothing is hard-coded.

---

## Hardware

| Part | Notes |
|---|---|
| **Seeed Studio XIAO ESP32-C3** | RISC-V MCU, WiFi, no PSRAM (~400 KB SRAM) |
| **2.8" ILI9341 TFT, 320×240** | SPI display, driven in landscape (rotation 1) |
| **XPT2046 touch controller** | Resistive touch, shares the display's SPI bus |
| **MAX98357A I2S amplifier** | Mono class-D amp for the voice prompts |
| **Speaker, 4–8 Ω** | Driven by the MAX98357A |
| **LiPo battery + resistor divider** | 2×100 kΩ divider (ratio 0.5) into an ADC pin |

The display and touch panel share one SPI bus (separate chip-select lines). The
backlight is on a dedicated GPIO (PWM-dimmable). Audio uses the ESP32's
hardware I2S peripheral, not a DAC.

---

## Wiring

### XIAO ESP32-C3 GPIO assignments

The SPI bus (MOSI / SCLK / MISO) is shared by the display and the touch
controller (separate chip-selects). SPI runs at 10 MHz (display) / 1 MHz
(touch) for reliability on this panel.

| Function | GPIO | XIAO pin |
|---|---|---|
| TFT MOSI (SDI) | GPIO10 | D10 |
| TFT SCLK | GPIO8 | D8 |
| TFT MISO (SDO) | GPIO9 | D9 |
| TFT CS | GPIO3 | D1 |
| TFT DC | GPIO4 | D2 |
| TFT RST | GPIO5 | D3 |
| Touch CS (T_CS) | GPIO6 | D4 |
| Touch data out (T_DO) → MISO | GPIO9 | D9 (via 10 kΩ, shared MISO) |
| Backlight (PWM) | GPIO7 | D5 |
| I2S BCLK | GPIO20 | D7 (RX) |
| I2S LRCLK (WS) | GPIO21 | D6 (TX) |
| I2S DOUT → amp DIN | GPIO2 | D0 (A0) |
| Battery charge sense (ADC) | GPIO2 | D0 (A0) |

Note that **D0 (GPIO2) appears twice** — audio DIN and the battery sense
divider are currently on the same pin. See the conflict note under
[Battery & charging](#battery--charging).

### External passive components

| Component | Placement | Purpose |
|---|---|---|
| 10 kΩ resistor | In series between XIAO MISO (GPIO9 / D9) and the panel's **T_DO** | Isolates the XPT2046's data-out on the shared SPI MISO line |
| 10 kΩ resistor | On the TFT **RST** line (GPIO5 / D3) | Reset pull-up (also the hook for later freeing GPIO5 via `TFT_RST=-1`) |
| 2 × 100 kΩ resistors | `VBAT → 100 kΩ → [sense node] → 100 kΩ → GND`; sense node → **D0** | Battery voltage divider (ratio 0.5) for state-of-charge |
| 1000 µF capacitor | Across XIAO **3V3 ↔ GND** | Bulk supply reservoir (steadies the 3V3 rail against display/audio current spikes) |

### Audio — MAX98357A I2S amplifier

| MAX98357A pin | Connects to | Notes |
|---|---|---|
| DIN | XIAO GPIO2 / D0 | I2S data |
| BCLK | XIAO GPIO20 / D7 | Bit clock |
| LRC | XIAO GPIO21 / D6 | Word / LR clock |
| VIN | 5V (or 3V3) | Louder at 5 V |
| GND | GND | |
| SD | Tie **high** (to VIN / 3V3) | Enables the amp; floating / low = shutdown |
| GAIN | Leave floating | Default 9 dB (tie GND = 15 dB, VIN = 3 dB) |
| Speaker ± | 4–8 Ω speaker | Mono output |

No external resistors are required for the MAX98357A — `SD` just needs to be
held high and `GAIN` can be left floating.

### Battery & charging

A single-cell LiPo powers the device through the XIAO's onboard battery pads
(charged over USB). State-of-charge is measured by the 100 kΩ / 100 kΩ divider
above, feeding the sense node into **D0 (GPIO2)**; the firmware maps
3.00 V → 0 % and 4.20 V → 100 % (ADC at 11 dB attenuation, 0–3.1 V range,
16-sample averaging).

> **Known pin conflict (pending final wire-up, left as-is for bench testing):**
> the battery sense divider and the I2S audio DIN are **both on D0 (GPIO2)** —
> they cannot share the pin — and the firmware currently reads the battery on
> **GPIO1**, not D0, so the on-screen battery percentage is not yet
> trustworthy. Planned fix at final assembly: the TFT RST 10 kΩ lets us tie RST
> to 3V3 and set `TFT_RST=-1` to free **GPIO5**; move I2S DOUT to GPIO5, and set
> the battery ADC pin to GPIO2 (D0 / A0, a reliable ADC1 input). Audio and
> battery then stop colliding and the firmware reads the correct pin.

---

## Repository layout

Two independent PlatformIO projects:

```
debug_upload/              Main firmware
├── platformio.ini
├── src/
│   ├── main.cpp            Boot, main loop, touch dispatch, WiFi/AP bring-up
│   ├── config/             Config: persisted settings (NVS/Preferences)
│   ├── display/
│   │   ├── DisplayManager  All rendering, screens, overlays, touch mapping,
│   │   │                   dirty-rectangle partial-update compositor
│   │   ├── BacklightManager PWM backlight + idle auto-dim
│   │   ├── ClothingAdvisor  Weather → clothing recommendation
│   │   └── Widget.h*        Retained-mode widget/Rect framework (see notes)
│   ├── tasks/TaskScheduler  Schedule model, active-task tracking, events
│   ├── audio/AudioManager   I2S WAV playback + synthesized touch beep
│   ├── sensors/BatteryMonitor  ADC → battery %
│   └── network/WebServer    Async HTTP + WebSocket (portal + dashboard + API)
├── data/                   LittleFS image contents
│   ├── index.html          STA dashboard
│   ├── config.html         Settings form
│   ├── setup.html          AP captive-portal WiFi setup
│   ├── schedule.json        Default schedule
│   └── audio/              starting/halfway/onemin/done .wav (generated)
└── tools/generate_audio.py Regenerate the voice prompts

touch_calibration_tool/    One-off utility: run once to calibrate the
                           resistive touch panel, then reflash the main firmware
```

\* `Widget.h` currently lives in `debug_upload/partial_render_dropin/display/`
and should be copied into `src/display/`; it is scaffolding for a future
widget-object refactor and is not yet referenced by the build.

---

## Software architecture

### Rendering & the partial-update compositor

The panel only addresses correctly at TFT_eSPI rotation 1 (320×240). All UI is
drawn in a **240×320 logical (portrait) off-screen sprite**, then mapped onto
the real landscape panel. Rather than pushing the whole frame every tick, the
firmware uses a **dirty-rectangle compositor**:

- Each screen region (status bar, weather row, task card, nav bar, task list,
  timer screens) is redrawn and pushed **only when its own source data
  changed** (change-detection via cached signatures).
- A single rotation-aware primitive, `_pushRegion(lx, ly, w, h)`, maps one
  logical rectangle onto the rotated panel and pushes just that sub-region
  (row-buffered, no full-frame allocation). This is the only place the 90°/270°
  rotation is handled.
- `setScreen()` and overlay dismissal force a one-shot full repaint so
  transitions always paint cleanly; while an overlay owns the screen, the
  underlying screen is never repainted.

The result: the animated task card pushes its region each tick, but static
strips (clock/weather/nav) push only on real changes, and the timer-set screen
paints once on entry.

### Screens & overlays

- **Screens:** `HOME`, `TASK_LIST`, `TIMER_SET`, `TIMER_RUNNING`
  (`enum class Screen`).
- **Overlays** (drawn on top, dismiss on tap): clothing "What to Wear",
  IP-address toast, and the task-complete rocket-launch celebration. Overlays
  wait for the opening tap to lift before a touch can dismiss them.

Text is drawn with TFT_eSPI's built-in GLCD bitmap font (no custom fonts
compiled in); all icons/graphics are hand-drawn primitives (rects, circles,
triangles, arcs, wide-lines) — there is no image decoder in the firmware.

### Task scheduling

`TaskScheduler` loads `schedule.json` into groups of tasks, tracks the current
and next task against the wall clock (with `daily`/`weekdays`/`weekends`
recurrence), computes elapsed/remaining/progress, and fires an event callback at
`START`, `MIDPOINT`, `ONE_MINUTE`, and `COMPLETE`.

### Audio

`AudioManager` streams mono 16-bit PCM WAV files from LittleFS through the
ESP32 hardware I2S driver (`driver/i2s.h`) to the MAX98357A, and can synthesize
a short square-wave "tap" beep directly into the I2S buffer. (ESP8266Audio is
deliberately excluded — it won't compile on the C3 core.)

### Connectivity & config

`Config` persists settings in NVS (Preferences). On boot, if no WiFi SSID is
saved the device starts a **SoftAP + captive portal**; otherwise it joins the
saved network. `WebServer` (ESPAsyncWebServer) serves the setup portal, the
settings form, the live dashboard, and a JSON/WebSocket status API. Weather is
fetched from OpenWeatherMap and cached to survive reboots.

### Boot sequence (`setup()`)

Init NVS (reformat if corrupt) → mount LittleFS (format on failure) → load
config → init battery / display / audio / backlight → load cached weather →
load schedule + register event callback → start AP or STA → start web server.

---

## Build & flash

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).

```bash
cd debug_upload

# Build
pio run

# Flash firmware over USB
pio run --target upload

# Build & upload the LittleFS data image (HTML, schedule.json, audio/)
pio run --target uploadfs
```

`uploadfs` is required at least once (and whenever you change anything under
`data/`) — the web UI, default schedule, and voice prompts all live in the
LittleFS image, separate from the firmware.

Serial monitor: `pio device monitor` (115200 baud).

---

## First-time setup (WiFi & config)

1. On first boot (no saved WiFi), the device starts a WiFi access point and
   shows setup instructions on screen.
2. Join that AP from a phone/laptop; the captive portal (`setup.html`) opens.
   Select your network and enter the password.
3. The device reboots onto your network. Tap the on-screen WiFi icon to see its
   IP address, then browse to it.
4. Open the settings page (`config.html`) to set **timezone** (POSIX TZ
   string), **city** and **OpenWeatherMap API key** (for weather), and
   **units** (metric/imperial).

---

## Touch calibration

The resistive panel needs a one-time calibration, done with the separate
`touch_calibration_tool` project so the main firmware never risks the library's
timeout-free calibration loop on a normal boot.

```bash
cd touch_calibration_tool
pio run --target upload         # flash the utility
pio device monitor              # optional, to watch progress
# Tap the 4 corner targets it draws.
# Then reflash the main firmware WITHOUT erasing flash:
cd ../debug_upload
pio run --target upload
```

Both projects use identical display/touch wiring and share the same NVS
namespace/key (`touchcal`/`cal`), so the main firmware picks up the calibration
automatically. **Do not "erase flash"** when reflashing, or the calibration is
wiped.

---

## Voice prompts (audio)

Four WAV files live in `data/audio/` and are played during tasks:

| File | Spoken |
|---|---|
| `starting.wav` | "Starting your task now." |
| `halfway.wav` | "You are halfway through." |
| `onemin.wav` | "One minute remaining." |
| `done.wav` | "Task complete. Well done." |

Format required by `AudioManager`: **mono, 16-bit PCM WAV** (8 kHz recommended;
the header's real sample rate is honored). The 44-byte header must be
standard — no metadata chunks before the audio data.

Regenerate them with `tools/generate_audio.py`:

```bash
# Offline, cross-platform (installs a bundled espeak-ng via pip):
python3 tools/generate_audio.py --engine espeak

# Best quality on macOS (built-in neural voices):
python3 tools/generate_audio.py --engine say --voice "Samantha"

# Neural, with a downloaded Piper model:
python3 tools/generate_audio.py --engine piper --piper-model voice.onnx
```

Requires `ffmpeg` on PATH for the final 8 kHz / mono / 16-bit conversion. Tasks
may override `starting`/`halfway`/`done` per-task via the schedule.

---

## Schedule format

`data/schedule.json` is a list of groups, each with tasks:

```json
{
  "groups": [
    {
      "id": "morning",
      "name": "Start of Day",
      "icon": "sunrise",
      "color": "#F4A460",
      "tasks": [
        {
          "id": "t1",
          "name": "Wake Up",
          "start_time": "06:00",
          "duration_min": 10,
          "audio_start": "/audio/starting.wav",
          "audio_mid":   "/audio/halfway.wav",
          "audio_done":  "/audio/done.wav",
          "recurrence":  "daily",
          "enabled":     true
        }
      ]
    }
  ]
}
```

- `start_time` is 24-hour `HH:MM`; `duration_min` is minutes.
- `recurrence` is `daily`, `weekdays`, or `weekends`.
- `audio_*` are optional per-task overrides; empty string uses the defaults.
- Group `color` is `#RRGGBB` (converted to the display's color format at draw
  time).

The schedule can be edited via the web UI or by re-uploading the LittleFS
image.

---

## Web dashboard & API

`WebServer` serves:

- `setup.html` — AP captive-portal WiFi picker.
- `config.html` — settings form (WiFi, timezone, city, API key, units).
- `index.html` — live dashboard.
- A **WebSocket** (`/ws`) that broadcasts a status JSON once per second: time,
  date, battery %, RSSI, mute state, current/next task with progress, weather,
  and any weather error.

---

## Memory notes

The C3 has ~400 KB SRAM and no PSRAM, so RAM is the tight resource.

- The off-screen canvas sprite is the dominant allocation. It runs at **8-bit
  color depth (~75 KB)**; 16-bit would double that to ~150 KB. 8-bit uses
  TFT_eSPI's 3-3-2 format — visually close for this flat-color UI, with blues
  quantized most coarsely. Revert `setColorDepth(8)` → `16` in
  `DisplayManager::begin()` if fidelity is ever preferred over RAM.
- The 1 Hz status broadcast reuses a single `JsonDocument` instead of
  allocating one per second, reducing heap churn/fragmentation over long
  uptimes.
- Per-frame drawing uses stack `char[]` buffers rather than heap `String`s on
  the hot path.
- The large bitmap fonts (`LOAD_FONT6/7/8`) live in flash, not RAM.

---

## Troubleshooting

- **Text doesn't render, only shapes do:** a font isn't compiled in. This
  build defines `LOAD_GLCD` (+ FONT2/4/6/7/8) via `build_flags`; without them,
  `drawString`/`print` silently no-op.
- **Touch is offset / inaccurate:** run the `touch_calibration_tool` (above).
  Don't erase flash when reflashing the main firmware afterward.
- **Colors look slightly off:** expected consequence of the 8-bit canvas (see
  Memory notes); revert to 16-bit if needed.
- **Weather row shows an error:** check the OpenWeatherMap API key and city in
  `config.html` (the dashboard surfaces the specific error).
- **Web UI / schedule / audio missing after flashing firmware:** run
  `pio run --target uploadfs` to write the LittleFS data image.
- **No audio:** confirm the WAVs are mono 16-bit PCM with a clean 44-byte
  header (regenerate with `tools/generate_audio.py`), the device isn't muted,
  and the MAX98357A `SD` pin is held high.
