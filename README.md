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
| **LiPo battery + resistor divider** | 2×200 kΩ divider (ratio 0.5) into an ADC pin |

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
| TFT RST | (no GPIO) | tied to 3.3 V via 10 kΩ; SPI software reset (`TFT_RST=-1`) |
| Touch CS (T_CS) | GPIO6 | D4 |
| Touch data out (T_DO) → MISO | GPIO9 | D9 (via 10 kΩ, shared MISO) |
| Backlight (PWM) | GPIO7 | D5 |
| I2S BCLK | GPIO20 | D7 (RX) |
| I2S LRCLK (WS) | GPIO21 | D6 (TX) |
| I2S DIN (amp data) | GPIO5 | D3 |
| Battery charge sense (ADC) | GPIO2 | D0 (A0) |

Freeing the TFT RST pin (tie to 3.3 V + SPI software reset) opened up GPIO5 for
the I2S audio data line, so battery sense (D0/GPIO2, a reliable ADC1 input) and
audio no longer share a pin.

The touch controller shares the display's SPI clock and data-in lines:
**T_CLK → GPIO8** (with SCLK) and **T_DIN → GPIO10** (with MOSI). Its interrupt
line **T_IRQ is left unconnected** — the firmware polls touch.

### Display module — 14-pin header

The common 2.8" ILI9341 + XPT2046 "red board" ordering (verify against your
exact module — some reorder LED/RESET or use a 5 V VCC):

| Pin | Module label | Wired to |
|---|---|---|
| 1 | VCC | 3.3 V |
| 2 | GND | GND |
| 3 | CS | GPIO3 / D1 |
| 4 | RESET | 3.3 V via 10 kΩ (software reset) |
| 5 | DC / RS | GPIO4 / D2 |
| 6 | SDI (MOSI) | GPIO10 / D10 |
| 7 | SCK | GPIO8 / D8 |
| 8 | LED (backlight) | GPIO7 / D5 |
| 9 | SDO (MISO) | GPIO9 / D9 |
| 10 | T_CLK | GPIO8 / D8 (shared SCK) |
| 11 | T_CS | GPIO6 / D4 |
| 12 | T_DIN | GPIO10 / D10 (shared MOSI) |
| 13 | T_DO | GPIO9 / D9 via 10 kΩ (shared MISO) |
| 14 | T_IRQ | not connected |

### Power & battery connector

- **3.3 V** (XIAO `3V3` pad) feeds: display VCC, MAX98357A `VIN` (see the Audio
  table for the 3.3 V vs 5 V trade-off), the RST 10 kΩ pull-up, the battery
  divider top, and the 1000 µF bulk cap (+).
- **GND** is common to the XIAO, display, amp, speaker return, divider bottom,
  and cap (−).
- **Battery:** a single-cell LiPo on a **JST-PH 2-pin** connector goes to the
  XIAO's underside **BAT+ / BAT−** pads (the XIAO's on-module charger charges it
  over USB). `+BATT` also feeds the sense divider → D0.
- **Speaker:** the MAX98357A `OUT+ / OUT−` drive a **4–8 Ω** speaker (a 2-pin
  JST-PH on the carrier PCB).

### External passive components

| Component | Placement | Purpose |
|---|---|---|
| 10 kΩ resistor | In series between XIAO MISO (GPIO9 / D9) and the panel's **T_DO** | Isolates the XPT2046's data-out on the shared SPI MISO line |
| 10 kΩ resistor | TFT **RST** → 3.3 V | Holds RST high; firmware uses the SPI software reset (`TFT_RST=-1`), which frees GPIO5 for the I2S data line |
| 2 × 200 kΩ resistors | `VBAT → 200 kΩ → [sense node] → 200 kΩ → GND`; sense node → **D0** | Battery voltage divider (ratio 0.5) for state-of-charge (200 kΩ over 100 kΩ halves idle drain) |
| 1000 µF capacitor | Across XIAO **3V3 ↔ GND** | Bulk supply reservoir (steadies the 3V3 rail against display/audio current spikes) |

### Audio — MAX98357A I2S amplifier

| MAX98357A pin | Connects to | Notes |
|---|---|---|
| DIN | XIAO GPIO5 / D3 | I2S data |
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
(charged over USB). The XIAO ESP32-C3 **charges** the cell but its battery pad
is **not connected to any ADC**, so there is no built-in way to read the
voltage — an external divider is required. State-of-charge is measured by the
200 kΩ / 200 kΩ divider above, feeding the sense node into **D0 (GPIO2)**; the
firmware reads it with `analogReadMilliVolts()` (factory-calibrated ADC) and
maps 3.00 V → 0 % and 4.20 V → 100 % (11 dB attenuation, 16-sample averaging).

The earlier D0 pin conflict is resolved on this board revision: TFT RST is tied
to 3.3 V (SPI software reset, `TFT_RST=-1`), which freed GPIO5 for the I2S audio
data line, so audio (GPIO5) and battery sense (D0/GPIO2) no longer collide. The
firmware reads the battery on D0/GPIO2 (`BatteryMonitor::begin(2)`) to match.
The `touch_calibration_tool` project uses the same `TFT_RST=-1`, so calibrate
and flash in that order without erasing flash.

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
   shows a **Wi-Fi Setup screen** on the TFT with the network name
   (`ScheduleTracker-Setup`), password (`setup1234`), and the portal address
   (`192.168.4.1`). The backlight stays on and touch navigation is disabled
   while in this mode.
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
| `starting.wav` | "Let's get started! You can do it." |
| `halfway.wav` | "You're halfway there. Keep it up!" |
| `onemin.wav` | "Almost done! Just one more minute." |
| `done.wav` | "All done! Great job!" |

Format required by `AudioManager`: **mono, 16-bit PCM WAV** (16 kHz default —
8 kHz sounds crackly; the header's real sample rate is honored). The 44-byte
header must be standard — no metadata chunks before the audio data.

Regenerate them with `tools/generate_audio.py`:

```bash
# Best quality — free neural voices, no API key (needs internet). A warm
# child voice, ideal for this device:
pip install edge-tts
python3 tools/generate_audio.py --engine edge --voice en-US-AnaNeural

# AWS Polly neural (needs an AWS account; uses your own credentials via
# `aws configure`). "Ivy" is a US-English child voice:
pip install boto3
python3 tools/generate_audio.py --engine polly --voice Ivy

# Natural, fully offline on macOS (built-in / downloaded "Enhanced" voices):
python3 tools/generate_audio.py --engine say --voice "Samantha"

# Offline everywhere, no internet — softened espeak (synthetic, last resort):
python3 tools/generate_audio.py --engine espeak --soft
```

Requires `ffmpeg` on PATH for the final mono / 16-bit / 16 kHz conversion. Tasks
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
