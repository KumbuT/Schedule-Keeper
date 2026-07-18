#  Schedule Keeper

A battery-powered daily schedule tracker built on the Seeed **XIAO ESP32-C3**. It drives a 2.8" touchscreen display, speaks voice prompts as tasks start and finish, shows live weather with a "what to wear" recommendation, and exposes a WebSocket dashboard so you can check on it from your phone.

Firmware is Arduino/PlatformIO C++; the on-device web UI is plain HTML/CSS/JS served from LittleFS.

---

## Contents

- [Features](#features)
- [Hardware](#hardware)
- [How it works](#how-it-works)
- [Web interface](#web-interface)
- [Project layout](#project-layout)
- [Building and flashing](#building-and-flashing)
- [Schedule format](#schedule-format)
- [First boot / WiFi setup](#first-boot--wifi-setup)
- [Known gaps](#known-gaps)

---

## Features

- **2.8" ILI9341 touch display** (240×320, SPI) with an Android-style status bar — WiFi signal arcs, battery percentage, time and date
- **Live weather** from OpenWeatherMap, refreshed every 20 minutes and cached to flash so the display shows real data immediately on boot, even before WiFi reconnects
- **"What to wear" overlay** — tapping the weather row runs a rule-based advisor (temperature, wind, humidity, rain/snow/storm) and lists recommended clothing
- **Grouped task schedule** — tasks are organized into named groups (e.g. *Start of Day*, *Work Block*) with per-task start time, duration and recurrence (`daily`, `weekdays`, `weekends`)
- **Countdown dial** — a 270° arc shows time remaining for the active task, colored cyan → yellow → red as the deadline approaches, with a small mood face and remaining time inside it
- **Voice prompts over I2S** — a MAX98357A amplifier speaks at task start, halfway, one-minute-remaining and completion; each task can override the default clips
- **Touch feedback beep** — generated directly as an I2S tone, no audio file needed
- **Auto-dimming backlight** — fades out after inactivity, wakes instantly on touch
- **Mute toggle** — from the touchscreen or the web dashboard
- **Captive-portal WiFi setup** — on first boot (or after a failed WiFi connection) the device becomes an access point with a setup page for SSID/password/timezone/city/weather key
- **Live web dashboard** — a WebSocket-driven page mirrors the physical display from any browser on the same network, plus a settings page for timezone/units/city/weather key
- **Battery monitoring** — voltage-divider ADC reading mapped to a percentage, for a LiPo cell charged externally (e.g. TP4056/MCP73831 module)
- **LittleFS storage** — schedule, cached weather, and web assets all live on the ESP32's flash filesystem

## Hardware

| Role | Part | Interface |
|---|---|---|
| MCU | Seeed XIAO ESP32-C3 | — |
| Display | 2.8" ILI9341 TFT, 240×320 | SPI |
| Touch | XPT2046 resistive touch controller | SPI (shared bus) |
| Audio amp | MAX98357A I2S Class-D amplifier | I2S |
| Speaker | Small 4–8Ω speaker matched to the amp's gain setting | analog, driven by amp |
| Battery | Single-cell LiPo | voltage divider → ADC |
| Charger | Any TP4056/MCP73831-style USB LiPo charger board | — |

### Pin assignments (XIAO ESP32-C3)

```
GPIO 8  (SCK)   SPI clock      → TFT SCLK, touch SCLK
GPIO 9  (MISO)  SPI MISO       → TFT MISO, touch MISO
GPIO 10 (MOSI)  SPI MOSI       → TFT MOSI, touch MOSI
GPIO 3          TFT chip select
GPIO 4          TFT data/command
GPIO 5          TFT reset
GPIO 6          Touch chip select
GPIO 7          TFT backlight (PWM, auto-dim)

GPIO 20         I2S bit clock   → MAX98357A BCLK
GPIO 21         I2S word select → MAX98357A LRCLK
GPIO 2          I2S data out    → MAX98357A DIN

GPIO 1  (ADC1)  Battery voltage divider midpoint
```

Battery sensing is a straightforward 100kΩ/100kΩ divider from the LiPo's positive terminal into GPIO1, referenced against the ESP32-C3's ~2.5V ADC range (`src/sensors/BatteryMonitor.h`). Full/empty thresholds are 4.20V / 3.00V.

Touch is polled (not interrupt-driven) at a 300ms debounce in the main loop, which is sufficient for this UI — see `DisplayManager::pollTouch()`.

The amplifier's default gain and the firmware's default output volume (`AudioManager` defaults to 1.0, i.e. full scale) will drive a small speaker hard. Match the speaker's power rating to the amp's output at your supply voltage, or lower `setVolume()` / the amp's GAIN pin if audio is distorted.

This repository does not include a 3D-printable enclosure — the case is left as an exercise for the builder, sized around the display and battery you choose.

## How it works

**Boot sequence** (`src/main.cpp`):

1. Mount LittleFS (formats on first boot if unmountable).
2. Load `DeviceConfig` from NVS (`Config::load()`).
3. Initialize battery monitor, display, audio, and backlight manager.
4. Load the last cached weather reading from flash so the display isn't blank while WiFi reconnects.
5. Load `/schedule.json` into the task scheduler.
6. If no WiFi SSID is saved, start a captive-portal access point; otherwise connect as a station. A failed station connection falls back to the access point automatically.
7. Start the web server (serves both the captive portal and the normal dashboard, depending on mode).

**Main loop**, once running:

- Every second: advance the task scheduler against wall-clock time, redraw the display, and broadcast status over WebSocket.
- Every 30 seconds: refresh the battery reading.
- Every 20 minutes (station mode only): re-fetch weather, subject to backoff rules described below.
- Every loop iteration: tick the backlight's idle timer, poll touch input (300ms debounce), and pump the audio state machine.

**Weather fetch** talks to OpenWeatherMap's current-conditions endpoint and reacts differently depending on the failure:

| Response | Behavior |
|---|---|
| `200` | Update display, persist to `/weather_cache.json` |
| `401` | API key invalid — stop retrying until the key is changed in `/config.html` |
| `404` | City not found — stop retrying until the city is changed |
| `429` | Rate limited — back off 60 minutes |
| `5xx` | Server error — back off 10 minutes |
| connection failure | Back off 2 minutes, keep showing the last valid reading |

**Task events.** For each active task the scheduler fires `START`, `MIDPOINT`, `ONE_MINUTE`, and `COMPLETE` events exactly once per occurrence. `main.cpp` wires these to `AudioManager::play()`, using a per-task audio path if one is set in `schedule.json`, otherwise a default clip (`/audio/starting.wav`, `halfway.wav`, `onemin.wav`, `done.wav`).

## Web interface

Three static pages are served from LittleFS (`data/`):

- **`index.html`** — the live dashboard. Connects to `ws://<device-ip>/ws`, mirrors time/battery/WiFi/weather/current task, and reproduces the same clothing-advisor logic as the firmware so the recommendation shows up in the browser too. Includes a mute button and a link to the config page.
- **`config.html`** — timezone/units/city/weather-key settings (with a masked key + "Replace" flow so the raw key is never re-sent unnecessarily), and a read-only view of the current task groups/tasks. Adding new groups/tasks from this page is a stub today — see [Known gaps](#known-gaps).
- **`setup.html`** — the captive-portal first-run form (SSID, password, timezone, city, weather key), served when the device is in access-point mode.

REST/WebSocket surface (`src/network/WebServer.cpp`):

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | Static files (dashboard by default) |
| `/ws` | WS | Live status push (1/sec) + `mute_toggle` / `reload_schedule` commands |
| `/api/status` | GET | One-shot JSON status snapshot |
| `/api/schedule` | GET / POST | Read or overwrite `/schedule.json` (POST reloads the scheduler immediately) |
| `/api/config` | GET / POST | Read/update timezone, city, units, weather key (masked on read) |
| `/api/mute` | POST | Toggle mute |
| `/save-setup` | POST | Captive-portal setup: validates and saves WiFi/timezone/city/key, then reboots |

Any unrecognized path in access-point mode redirects to `/setup.html`, which is what makes the captive portal pop up automatically on phones/laptops.

## Project layout

```
Schedule-Keeper/
├── platformio.ini
├── data/                       # uploaded to LittleFS as the device filesystem
│   ├── index.html              # live dashboard
│   ├── config.html             # settings + schedule viewer
│   ├── setup.html              # captive-portal first-run form
│   ├── schedule.json           # task data (read/written via /api/schedule)
│   └── audio/
│       └── README.md           # expected WAV filenames/format (files not checked in)
├── src/
│   ├── main.cpp                 # boot flow, main loop, weather fetch, task→audio wiring
│   ├── config/                  # Config: NVS-backed WiFi/timezone/city/weather-key/mute state
│   ├── display/
│   │   ├── DisplayManager        # TFT drawing, touch polling, screens
│   │   ├── ClothingAdvisor       # weather → clothing rules
│   │   └── BacklightManager      # PWM auto-dim + wake-on-touch
│   ├── network/
│   │   └── WebServer             # ESPAsyncWebServer routes + WebSocket
│   ├── tasks/
│   │   └── TaskScheduler          # groups/tasks, active-task lookup, event firing
│   ├── audio/
│   │   └── AudioManager           # I2S playback + generated touch beep
│   └── sensors/
│       └── BatteryMonitor         # voltage divider → percentage
├── tools/
│   └── generate_audio.py       # PC-side script: text → WAV prompts via gTTS + ffmpeg
├── lib/                        # PlatformIO private-library folder (boilerplate README only, unused)
└── test/                       # PlatformIO unit-test folder (boilerplate README only, no tests yet)
```

## Building and flashing

**Prerequisites:** [PlatformIO](https://platformio.org/) (either the CLI or the VS Code extension), a USB-C cable, and the parts listed under [Hardware](#hardware) wired per the pin table above.

1. Clone the repository and open it in PlatformIO.
2. Generate the voice prompt WAV files before your first upload — they are **not** checked into the repo:
   ```bash
   pip install gTTS
   # ffmpeg must also be on your PATH
   python tools/generate_audio.py
   ```
   This writes `starting.wav`, `halfway.wav`, `onemin.wav`, and `done.wav` into `data/audio/` (8kHz mono 16-bit PCM, per `data/audio/README.md`). Swap in your own recordings if you'd rather not use text-to-speech, keeping the same filenames and format.
3. Upload the filesystem image first (**PlatformIO → Project Tasks → Build Filesystem Image**, then **Upload Filesystem Image**) so `data/` — including the schedule and audio — lands on the device's flash.
4. Build and upload the firmware (**Upload**).
5. Open the serial monitor at 115200 baud to watch boot logs, WiFi status, and weather fetch results.

The `platformio.ini` build deliberately excludes the `ESP8266Audio` library — it pulls in a MIDI generator that fails to compile on the ESP32-C3's RISC-V core — and instead drives the MAX98357A directly through ESP-IDF's `driver/i2s.h`.

## Schedule format

`data/schedule.json` (also readable/writable at runtime via `GET`/`POST /api/schedule`) is a list of groups, each containing tasks:

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

`recurrence` accepts `daily`, `weekdays`, or `weekends`. Leaving an `audio_*` field empty falls back to the corresponding default clip. `color` is a `#RRGGBB` hex string used for the group's accent color on-device and in the dashboard.

## First boot / WiFi setup

On first boot (no saved SSID) or after a failed WiFi connection, the device starts a SoftAP named **`ScheduleTracker-Setup`** (password `setup1234`) and serves the captive portal at `192.168.4.1`. Most phones/laptops will pop the setup page open automatically; if not, connect to the AP and browse to `192.168.4.1` manually.

The setup form validates SSID/password length, requires a city name for weather, and loosely validates the OpenWeatherMap key format (32 hex characters) — an odd-looking key is still saved (with a warning) so a typo doesn't force you to redo the whole WiFi setup, and you can fix it afterward from `/config.html`. Submitting reboots the device into station mode.

## Known gaps

Documenting these honestly rather than glossing over them:

- **Adding new task groups/tasks from `config.html` is not wired up yet** — the page lists existing groups/tasks read-only and has a "＋ Add Group" button that's currently a stub. Editing `schedule.json` directly (or via `POST /api/schedule`) is the reliable path today.
- **No audio files are checked into the repo** — run `tools/generate_audio.py` (or supply your own WAVs) before uploading the filesystem image, or the device will simply skip playback for missing files.
- **No enclosure files included** — case dimensions depend on whichever display/battery/speaker combination you source.
- **Touch is polling-based**, not interrupt-driven; fine for this UI's needs but worth knowing if you extend it.
