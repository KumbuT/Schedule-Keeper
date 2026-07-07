# ESP32-C3 Portable Schedule Tracker

A compact, battery-powered schedule tracker built on the Seeed XIAO ESP32-C3 featuring a 2.8" touch TFT display, voice audio prompts, captive portal WiFi setup, real-time weather, WebSocket-based web dashboard, and a 3D-printed enclosure.

---

## Table of Contents

1. [Features](#features)
2. [Bill of Materials](#bill-of-materials)
3. [Hardware Wiring](#hardware-wiring)
4. [3D Printed Case](#3d-printed-case)
5. [Firmware Architecture](#firmware-architecture)
6. [Project Structure](#project-structure)
7. [PlatformIO Setup](#platformio-setup)
8. [Full Source Code](#full-source-code)
   - [platformio.ini](#platformioini)
   - [Config](#config)
   - [TaskScheduler](#taskscheduler)
   - [DisplayManager](#displaymanager)
   - [WebServer](#webserver)
   - [AudioManager](#audiomanager)
   - [BatteryMonitor](#batterymonitor)
   - [main.cpp](#maincpp)
9. [Web Portal](#web-portal)
10. [Schedule JSON Format](#schedule-json-format)
11. [Audio File Preparation](#audio-file-preparation)
12. [First Boot & Setup Flow](#first-boot--setup-flow)
13. [Development Phases](#development-phases)

---

## Features

- **2.8" ILI9341 touch TFT** — 240×320 portrait display with XPT2046 resistive touch
- **Android-style status bar** — WiFi arc signal strength, battery percentage, time, date
- **Live weather** — OpenWeatherMap current conditions (temp, humidity, wind, description)
- **Grouped task scheduling** — Tasks organised into named groups (e.g. *Start of Day*, *Work Block*)
- **Voice audio prompts** — I2S MAX98357A amplifier plays cues at task start, halfway, 1-minute warning, and completion
- **Mute toggle** — Silence audio from touch or web portal; auto-unmute option
- **Captive portal WiFi setup** — SoftAP on first boot, DNS redirect, web form for SSID/password/timezone/weather config
- **WebSocket web dashboard** — Live status mirroring the physical display, accessible from any browser on the same network
- **LiPo battery** — TP4056 USB-C charging with overdischarge protection, voltage-divider battery level reading
- **LittleFS storage** — Schedule JSON and audio files stored on flash; editable via web portal

---

## Bill of Materials

All prices verified July 2025. Links go directly to the product page.

---

### MCU

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Primary | **Seeed XIAO ESP32-C3** | $4.99 each | [seeedstudio.com](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html) |
| ✅ 3-pack (better value) | **Seeed XIAO ESP32-C3 3-Pack** | $13.49 ($4.50/unit) | [seeedstudio.com](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C3-3PCS-p-5920.html) |
| ✅ Pre-soldered headers | **Seeed XIAO ESP32-C3 (Pre-Soldered)** | ~$5.99 | [seeedstudio.com](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C3-Pre-Soldered-p-6331.html) |
| Also on Amazon | **XIAO ESP32-C3 (Amazon)** | ~$5.99 | [amazon.com](https://www.amazon.com/Seeed-Studio-XIAO-ESP32C3-Microcontroller/dp/B0B94JZ2YF) |
| Alt (more RAM/GPIO) | **Seeed XIAO ESP32-S3** | $6.99 | [seeedstudio.com](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html) |

> **Tip:** Buy the 3-pack — keep one for firmware development while the other lives in the case.

---

### Display — 2.8" ILI9341 + XPT2046 Touch (SPI)

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Best value | **HiLetgo 2.8" ILI9341 + XPT2046 Touch** | ~$9–11 | [amazon.com](https://www.amazon.com/HiLetgo-240X320-Resolution-Display-ILI9341/dp/B073R7BH1B) |
| ✅ With setup guide | **ShillehTek 2.8" ILI9341 + XPT2046 + microSD** | $10.99 | [shillehtek.com](https://shillehtek.com/products/2-8-spi-tft-lcd-touch-screen-module-240x320-ili9341-for-arduino) |
| Budget | **Generic 2.8" ILI9341 SPI TFT + Touch Pen** | ~$8 | [amazon.com](https://www.amazon.com/Display-Screen-Module-ILI9341-320X240/dp/B0BZGXY57J) |

> All three use the ILI9341 driver and XPT2046 touch controller — fully compatible with TFT_eSPI and the firmware in this project. They operate at 3.3V–5V and need no level shifter with the XIAO C3's 3.3V GPIO. The ShillehTek version includes a full wiring + code guide, which is handy for first-time bring-up.

---

### I2S Audio Amplifier

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Primary | **Adafruit MAX98357A I2S 3W Class D Amp (#3006)** | $5.95 | [adafruit.com](https://www.adafruit.com/product/3006) |
| Same via DigiKey | Adafruit #3006 | in stock | [digikey.com](https://www.digikey.com/en/products/detail/adafruit-industries-llc/3006/6058477) |
| Budget | **Generic MAX98357A breakout** | ~$1.50 | AliExpress: search `MAX98357A` |

> Adafruit's board comes fully assembled with a pre-soldered screw terminal block for the speaker wires. Default gain is 9dB (float GAIN pin). Connect GAIN to GND for 6dB if output is distorted at the 0.5W speaker rating. The MAX98357A can push up to 3.2W into 4Ω at 5V — use a 4Ω speaker or limit the gain to avoid blowing an 8Ω 0.5W speaker.

---

### Speaker

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Primary | **Adafruit Mini Oval Speaker 8Ω 1W with Short Wires (#4227)** | $1.95 | [adafruit.com](https://www.adafruit.com/product/4227) |
| Alt (longer wire) | **Adafruit Mini Oval Speaker 8Ω 1W (#3923)** | $1.95 | [adafruit.com](https://www.adafruit.com/product/3923) |
| ⚠️ Avoid with MAX98357A | ~~Adafruit Mini Metal Speaker 8Ω 0.5W (#1890)~~ | $1.95 | — |

> **Do not use the 0.5W #1890 with the MAX98357A at default gain.** At 3.3V the amp delivers ~0.9W into 8Ω — nearly 2× the speaker's rating. At 5V it can push 1.7W, which will blow it out. The 1W-rated #4227 is the same price and Adafruit explicitly lists it as compatible with the MAX98357A. It comes with a short ~10mm wire — cut the Molex connector off and use bare wires into the amp's screw terminal.
>
> In `AudioManager.cpp`, set a safe gain ceiling regardless of which speaker you use:
> ```cpp
> _out->SetGain(0.6f);  // 60% — keeps peak power under 1W at 3.3V
> ```
>
> If you already have the #1890 and don't want to replace it, connect the MAX98357A GAIN pin directly to GND — this drops gain to 6dB and limits peak output to ~0.45W, within the speaker's rating. Quieter, but safe and still clearly audible for voice prompts.

---

### LiPo Battery

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Primary | **Adafruit 400mAh LiPo 3.7V (#3898)** | $6.95 | [adafruit.com](https://www.adafruit.com/product/3898) |
| Higher capacity | **Adafruit 500mAh LiPo (#1578)** | $7.95 | [adafruit.com](https://www.adafruit.com/product/1578) |
| Also via DigiKey | Adafruit #3898 | in stock | [digikey.com](https://www.digikey.com/en/products/detail/adafruit-industries-llc/3898/9685336) |

> Adafruit batteries include a genuine JST-PH 2.0mm connector (not a knock-off — they click cleanly), built-in protection that cuts out at 3.0V, and polarity that matches all Adafruit charger boards. Do **not** substitute unbranded AliExpress cells for a device charging unattended inside a 3D-printed enclosure.

---

### LiPo Charger

| Option | Product | Price | Link |
|--------|---------|-------|------|
| ✅ Primary | **Adafruit Micro-Lipo USB-C Charger MCP73831 (#4410)** | $5.95 | [adafruit.com](https://www.adafruit.com/product/4410) |
| Budget (3-pack) | **HiLetgo TP4056 USB-C with Dual Protection (3-pack)** | ~$8 | [amazon.com](https://www.amazon.com/HiLetgo-Lithium-Charging-Protection-Functions/dp/B07PKND8KG) |
| Budget (5-pack) | **Diymore TP4056 USB-C with Dual Protection (5-pack)** | ~$8 | [amazon.com](https://www.amazon.com/Lithium-Battery-Charging-Protection-Functions/dp/B07MDPLQ18) |

> The Adafruit #4410 uses the MCP73831 IC and has proper charge termination plus red/green status LEDs. It pairs cleanly with the Adafruit battery above. The TP4056 all-in-one modules work fine — the HiLetgo and Diymore variants above include dual protection (TP4056 + DW01A), with overcharge cutoff at 4.2V and overdischarge protection at 3.0–3.2V. Avoid bare TP4056 boards without the DW01A protection IC.

---

### Passives & Hardware

| Qty | Item | Notes | Where |
|-----|------|-------|-------|
| 2 | Resistor 100kΩ 1/4W | Battery voltage divider | Any — [Amazon 100-pack ~$5](https://www.amazon.com/s?k=100k+resistor+1%2F4w) |
| 1 | Capacitor 100µF 6.3V | 3V3 rail bulk decoupling | Any — [Amazon assorted ~$8](https://www.amazon.com/s?k=electrolytic+capacitor+kit) |
| 3 | Capacitor 0.1µF ceramic | Per-IC decoupling | Any — [Amazon ceramic kit ~$6](https://www.amazon.com/s?k=ceramic+capacitor+0.1uf) |
| 4 | M2×6mm screws + brass inserts | Case assembly | [Amazon M2 assortment ~$9](https://www.amazon.com/s?k=m2+brass+heat+set+inserts) |
| 1 | JST-PH 2.0mm cable pair | Battery connector | [Adafruit #3814 $0.75](https://www.adafruit.com/product/3814) |

---

### Total Build Cost

| Tier | Key choices | Est. Total |
|------|-------------|------------|
| **Quality** | Adafruit charger + battery, ShillehTek display, Adafruit amp/speaker | **~$42–45** |
| **Mid** | Adafruit battery, TP4056 charger, HiLetgo display, Adafruit amp | **~$32–35** |
| **Budget** | TP4056, generic display, AliExpress amp, off-brand battery | **~$18–22** |

> The quality build costs less than a meal out and will save hours of debugging. The biggest risk in budget builds is the battery — don't compromise there.

### Alternative MCU

| Part | Notes |
|------|-------|
| Seeed XIAO ESP32-S3 ($6.99) | More GPIO headroom, larger RAM — useful if adding more peripherals |

---

## Hardware Wiring

### Pin Assignments — XIAO ESP32-C3

```
XIAO ESP32-C3 GPIO   Signal            Connected To
─────────────────────────────────────────────────────
GPIO 8  (SCK)        SPI Clock         TFT SCLK, Touch SCLK
GPIO 9  (MISO)       SPI MISO          TFT MISO, Touch MISO
GPIO 10 (MOSI)       SPI MOSI          TFT MOSI, Touch MOSI
GPIO 3               TFT Chip Select   TFT CS
GPIO 4               TFT Data/Command  TFT DC
GPIO 5               TFT Reset         TFT RST
GPIO 6               Touch Chip Select Touch CS
GPIO 7               Touch IRQ         Touch IRQ (optional)

GPIO 20 (I2S BCLK)   I2S Bit Clock     MAX98357A BCLK
GPIO 21 (I2S LRCK)   I2S Word Select   MAX98357A LRCLK
GPIO 2  (I2S DOUT)   I2S Data Out      MAX98357A DIN

GPIO 1  (ADC1_0)     Battery ADC       Voltage divider midpoint

3V3                  Power             TFT VCC, Touch VCC, MAX98357A VDD
GND                  Ground            All grounds
5V (via 5V pin)      Charge input      TP4056 OUT+ → XIAO 5V pin
```

### Battery Circuit

```
LiPo (+) ──► TP4056/MCP73831 BAT+ ──► Protection circuit ──► XIAO 5V pin
LiPo (-) ──► TP4056/MCP73831 BAT- ──► GND

Battery level sense (voltage divider):
LiPo (+) ──► 100kΩ ──► GPIO 1 (ADC) ──► 100kΩ ──► GND

Max voltage at divider midpoint: 4.2V / 2 = 2.1V  (within ADC 2.5V range)
Min voltage at divider midpoint: 3.0V / 2 = 1.5V
```

### MAX98357A Wiring

```
MAX98357A    XIAO C3
─────────────────────
VIN    ──►  3V3
GND    ──►  GND
BCLK   ──►  GPIO 20
LRCLK  ──►  GPIO 21
DIN    ──►  GPIO 2
SD     ──►  3V3 (always enabled) or GPIO for software mute
GAIN   ──►  leave floating (9dB gain default)
```

> **Speaker impedance note:** MAX98357A supports 4Ω or 8Ω speakers. The Adafruit breakout defaults to 9dB gain. If audio is distorted, connect GAIN to GND (6dB) or 100kΩ to VDD (12dB).

---

## 3D Printed Case

### Dimensions

```
Overall:        75mm × 60mm × 22mm
Display window: 50mm × 38mm cutout (flush-mounted, no bezel lip)
Speaker grille: 28mm circular hex mesh on left side wall
USB-C port:     11mm × 4mm cutout on bottom edge (TP4056 access)
Reset button:   3mm hole on right side wall
```

### Layer Breakdown

```
┌─────────────────────────────────────────┐  Top shell (PETG, 1.8mm walls)
│  [Display window — flush, 0.5mm ledge]  │  Display PCB snaps onto internal shelf
│  [USB-C cutout on bottom edge]          │
│  [Reset button hole on side]            │
├─────────────────────────────────────────┤  Mid frame / PCB shelf
│  XIAO C3 — M2 brass inserts, 4mm posts  │  Heat-set inserts, 4× M2×6mm screws
│  MAX98357A amp — hot glue or standoffs  │
│  TP4056 module — aligned to USB-C hole  │
│  Wire channels routed between layers    │
├─────────────────────────────────────────┤  Battery bay
│  LiPo cell — friction fit + foam pad    │  3mm closed-cell foam adhesive pad
│  Voltage divider resistors              │
└─────────────────────────────────────────┘  Bottom shell
                                             Snap-fit clips or 2× M2 screws
```

### Printing Notes

- **Material:** PETG preferred (impact resistance, slight flex for snap clips). PLA works fine.
- **Layer height:** 0.2mm standard; 0.15mm for display window and speaker grille details.
- **Speaker grille:** Hex mesh at 1mm hex diameter, 0.4mm walls. Print at 0.15mm layer height minimum. Orient grille face-up.
- **Display window:** Print with 0.5mm internal ledge for display PCB to seat against. Tolerance: add 0.3mm clearance on all PCB edges.
- **Acoustic chamber:** Leave 8–10mm air gap behind the speaker cone inside the case for better bass response.
- **Speaker placement:** Mount on the side wall, firing forward (not downward) for better projection.
- **Snap fit tolerances:** Male clip 0.6mm narrower than female receiver for reliable engagement.

### Recommended Slicer Settings

```
Walls:          3 perimeters
Infill:         20% gyroid
Supports:       Only for USB-C overhang if needed
Brim:           5mm on bottom shell only
Temperature:    PETG 235°C nozzle / 70°C bed
```

---

## Firmware Architecture

### System State Machine

```
BOOT
 └─► Load config from NVS
      ├─ WiFi SSID empty?
      │    YES → AP_PORTAL mode
      │          SoftAP: "ScheduleTracker-Setup" / "setup1234"
      │          DNS: all queries → 192.168.4.1
      │          Serve captive portal (setup.html)
      │          User enters: SSID, password, timezone, city, OWM key
      │          POST /save-setup → write NVS → reboot
      │
      └─ NO  → STA_CONNECT mode
                WiFi.begin() → 15s timeout
                setenv("TZ", posix_tz) + tzset()
                configTime() → SNTP sync (pool.ntp.org)
                fetchWeather()
                → RUNNING mode

RUNNING (every 1 second)
 └─► getLocalTime() → struct tm
      TaskScheduler::tick(tm)
        ├─ Find active task by wall-clock window
        ├─ Calculate elapsed / remaining seconds
        └─ Fire events: START / MIDPOINT / ONE_MINUTE / COMPLETE
             └─► AudioManager::play() (if not muted)
      DisplayManager::update(tm)
        ├─ Status bar: WiFi arcs, battery, time, date
        ├─ Weather row: temp, description, icon
        ├─ Current task: name, group, progress bar, remaining time
        └─ Nav bar: All Tasks | Mute toggle
      AppWebServer::broadcastStatus()   ← WebSocket push to all clients
      pollTouch()
        ├─ Zone 1 (All Tasks button) → switch screen
        └─ Zone 2 (Mute button)     → toggle mute, save NVS

Every 20 minutes:
  fetchWeather() → update DisplayManager::weather struct

Every 5 seconds:
  _ws.cleanupClients()
```

### WebSocket Message Format

**Server → Client (pushed every 1 second):**
```json
{
  "type": "status",
  "time": "14:32:07",
  "date": "Monday 07 Jul 2025",
  "battery_pct": 84,
  "rssi": -62,
  "muted": false,
  "current_task": "Drink Coffee",
  "group": "Start of Day",
  "elapsed_sec": 142,
  "remaining_sec": 158,
  "duration_sec": 300,
  "progress_pct": 47.3,
  "next_task": "Wash Up",
  "weather": {
    "temp": 22.1,
    "desc": "Partly Cloudy",
    "icon": "02d",
    "humid": 60
  }
}
```

**Client → Server (commands):**
```json
{ "type": "mute_toggle" }
{ "type": "reload_schedule" }
```

---

## Project Structure

```
schedule-tracker/
├── platformio.ini
├── data/                          ← Uploaded to LittleFS
│   ├── index.html                 ← Live dashboard (WebSocket client)
│   ├── config.html                ← Schedule + device config editor
│   ├── setup.html                 ← Captive portal first-run page
│   ├── schedule.json              ← Task data (editable via portal)
│   └── audio/
│       ├── starting.raw           ← 8kHz 8-bit mono PCM
│       ├── halfway.raw
│       ├── onemin.raw
│       └── done.raw
└── src/
    ├── main.cpp
    ├── config/
    │   ├── Config.h
    │   └── Config.cpp
    ├── display/
    │   ├── DisplayManager.h
    │   └── DisplayManager.cpp
    ├── network/
    │   ├── WebServer.h
    │   └── WebServer.cpp
    ├── tasks/
    │   ├── TaskScheduler.h
    │   └── TaskScheduler.cpp
    ├── audio/
    │   ├── AudioManager.h
    │   └── AudioManager.cpp
    └── sensors/
        ├── BatteryMonitor.h
        └── BatteryMonitor.cpp
```

---

## PlatformIO Setup

### Prerequisites

1. Install [VS Code](https://code.visualstudio.com/)
2. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
3. Clone or create the project folder
4. Upload `data/` folder to LittleFS: **PlatformIO → Project Tasks → Build Filesystem Image → Upload Filesystem Image**

---

## Full Source Code

### `platformio.ini`

```ini
[env:seeed_xiao_esp32c3]
platform = espressif32
board = seeed_xiao_esp32c3
framework = arduino

board_build.filesystem = littlefs

lib_deps =
  bodmer/TFT_eSPI @ ^2.5.43
  ESP32Async/AsyncTCP @ ^3.3.8
  ESP32Async/ESPAsyncWebServer @ ^3.7.6
  bblanchon/ArduinoJson @ ^7.0.0
  fbiego/ESP32Time @ ^2.0.0

; ESP8266Audio is intentionally excluded — it pulls in AudioGeneratorMIDI
; which fails to compile on the ESP32-C3 RISC-V core. Audio is handled
; directly via the ESP-IDF I2S driver instead (driver/i2s.h).
lib_ignore =
  ESP8266Audio

monitor_speed = 115200
upload_speed = 921600

build_flags =
  -DASYNC_TCP_SSL_ENABLED=0
  -D CONFIG_ASYNC_TCP_RUNNING_CORE=1
  -D CONFIG_ASYNC_TCP_USE_WDT=0
  -D USER_SETUP_LOADED=1
  -D ILI9341_DRIVER=1
  -D TFT_MOSI=10
  -D TFT_SCLK=8
  -D TFT_CS=3
  -D TFT_DC=4
  -D TFT_RST=5
  -D TFT_MISO=9
  -D TOUCH_CS=6
  -D SPI_FREQUENCY=40000000
  -D SPI_TOUCH_FREQUENCY=2500000
```

> **Build flags explained:**
> - `ASYNC_TCP_SSL_ENABLED=0` — Disables TLS (not needed on local network, saves RAM)
> - `CONFIG_ASYNC_TCP_RUNNING_CORE=1` — Pins AsyncTCP task to core 1, prevents SPI bus starvation on the single-core C3
> - `CONFIG_ASYNC_TCP_USE_WDT=0` — Disables AsyncTCP watchdog for more stable task scheduling
> - `USER_SETUP_LOADED=1` — Tells TFT_eSPI to use build flags instead of its own `User_Setup.h`
> - **No ESP8266Audio** — Audio is handled via the native ESP-IDF `driver/i2s.h` which ships with the espressif32 platform. No extra library needed, no MIDI dependency, no RISC-V compatibility issues.

---

### Config

**`src/config/Config.h`**
```cpp
#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct DeviceConfig {
  String wifiSSID;
  String wifiPassword;
  String timezone;      // POSIX tz string e.g. "AEST-10AEDT,M10.1.0,M4.1.0/3"
  String city;          // City name for OpenWeatherMap
  String owmApiKey;     // OpenWeatherMap API key
  bool   metricUnits;   // true = Celsius/km, false = Fahrenheit/mph
  bool   muted;         // Audio mute state
};

class Config {
public:
  static Config& instance();
  void load();
  void save();
  void reset();

  DeviceConfig data;

private:
  Config() {}
  Preferences _prefs;
};
```

**`src/config/Config.cpp`**
```cpp
#include "Config.h"

Config& Config::instance() {
  static Config inst;
  return inst;
}

void Config::load() {
  _prefs.begin("device", true);
  data.wifiSSID     = _prefs.getString("ssid",    "");
  data.wifiPassword = _prefs.getString("pass",    "");
  data.timezone     = _prefs.getString("tz",      "UTC0");
  data.city         = _prefs.getString("city",    "London");
  data.owmApiKey    = _prefs.getString("owmkey",  "");
  data.metricUnits  = _prefs.getBool  ("metric",  true);
  data.muted        = _prefs.getBool  ("muted",   false);
  _prefs.end();
}

void Config::save() {
  _prefs.begin("device", false);
  _prefs.putString("ssid",   data.wifiSSID);
  _prefs.putString("pass",   data.wifiPassword);
  _prefs.putString("tz",     data.timezone);
  _prefs.putString("city",   data.city);
  _prefs.putString("owmkey", data.owmApiKey);
  _prefs.putBool  ("metric", data.metricUnits);
  _prefs.putBool  ("muted",  data.muted);
  _prefs.end();
}

void Config::reset() {
  _prefs.begin("device", false);
  _prefs.clear();
  _prefs.end();
}
```

---

### TaskScheduler

**`src/tasks/TaskScheduler.h`**
```cpp
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>

struct Task {
  String id;
  String name;
  int    startHour;
  int    startMin;
  int    durationMin;
  String audioStart;
  String audioMid;
  String audioDone;
  String recurrence;    // "daily" | "weekdays" | "weekends"
  bool   enabled;
  bool   completedToday;
};

struct TaskGroup {
  String id;
  String name;
  String icon;
  uint32_t color;       // RGB888 stored, convert to RGB565 when drawing
  std::vector<Task> tasks;
};

enum class TaskEvent { START, MIDPOINT, ONE_MINUTE, COMPLETE };

class TaskScheduler {
public:
  static TaskScheduler& instance();

  bool loadFromFile(const char* path = "/schedule.json");
  bool saveToFile  (const char* path = "/schedule.json");

  void tick(struct tm* now);    // Call every second from main loop

  const Task*      currentTask()  const { return _currentTask; }
  const Task*      nextTask()     const { return _nextTask; }
  const TaskGroup* currentGroup() const { return _currentGroup; }
  int   elapsedSec()   const { return _elapsedSec; }
  int   remainingSec() const { return _remainingSec; }
  float progressPct()  const;

  std::vector<TaskGroup> groups;

  using EventCallback = std::function<void(const Task&, TaskEvent)>;
  void onEvent(EventCallback cb) { _eventCb = cb; }

private:
  TaskScheduler() {}
  void _findActiveTask(struct tm* now);
  bool _taskActiveNow (const Task& t, struct tm* now) const;
  bool _taskDueToday  (const Task& t, struct tm* now) const;

  const Task*      _currentTask  = nullptr;
  const Task*      _nextTask     = nullptr;
  const TaskGroup* _currentGroup = nullptr;
  int  _elapsedSec   = 0;
  int  _remainingSec = 0;
  bool _firedStart   = false;
  bool _firedMid     = false;
  bool _firedOneMin  = false;
  bool _firedDone    = false;

  EventCallback _eventCb;
};
```

**`src/tasks/TaskScheduler.cpp`**
```cpp
#include "TaskScheduler.h"
#include <LittleFS.h>

TaskScheduler& TaskScheduler::instance() {
  static TaskScheduler inst;
  return inst;
}

bool TaskScheduler::loadFromFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.println("[TaskScheduler] schedule.json not found");
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[TaskScheduler] JSON parse error: %s\n", err.c_str());
    return false;
  }

  groups.clear();
  for (JsonObject g : doc["groups"].as<JsonArray>()) {
    TaskGroup grp;
    grp.id    = g["id"].as<String>();
    grp.name  = g["name"].as<String>();
    grp.icon  = g["icon"].as<String>();
    // Parse hex color string "#RRGGBB"
    String colorStr = g["color"] | "#00BFFF";
    grp.color = strtoul(colorStr.substring(1).c_str(), nullptr, 16);

    for (JsonObject t : g["tasks"].as<JsonArray>()) {
      Task task;
      task.id          = t["id"].as<String>();
      task.name        = t["name"].as<String>();
      String st        = t["start_time"].as<String>();  // "HH:MM"
      task.startHour   = st.substring(0, 2).toInt();
      task.startMin    = st.substring(3, 5).toInt();
      task.durationMin = t["duration_min"].as<int>();
      task.audioStart  = t["audio_start"] | "";
      task.audioMid    = t["audio_mid"]   | "";
      task.audioDone   = t["audio_done"]  | "";
      task.recurrence  = t["recurrence"]  | "daily";
      task.enabled     = t["enabled"]     | true;
      task.completedToday = false;
      grp.tasks.push_back(task);
    }
    groups.push_back(grp);
  }

  Serial.printf("[TaskScheduler] Loaded %d groups\n", groups.size());
  return true;
}

bool TaskScheduler::_taskDueToday(const Task& t, struct tm* now) const {
  if (t.recurrence == "daily")    return true;
  int dow = now->tm_wday;  // 0 = Sunday
  if (t.recurrence == "weekdays") return dow >= 1 && dow <= 5;
  if (t.recurrence == "weekends") return dow == 0 || dow == 6;
  return true;
}

bool TaskScheduler::_taskActiveNow(const Task& t, struct tm* now) const {
  if (!t.enabled || !_taskDueToday(t, now)) return false;
  int nowMin   = now->tm_hour * 60 + now->tm_min;
  int startMin = t.startHour  * 60 + t.startMin;
  int endMin   = startMin + t.durationMin;
  return nowMin >= startMin && nowMin < endMin;
}

void TaskScheduler::_findActiveTask(struct tm* now) {
  int nowMin    = now->tm_hour * 60 + now->tm_min;
  _currentTask  = nullptr;
  _currentGroup = nullptr;
  _nextTask     = nullptr;

  int nextDelta = INT_MAX;

  for (auto& grp : groups) {
    for (auto& task : grp.tasks) {
      if (!task.enabled || !_taskDueToday(const_cast<Task&>(task), now)) continue;

      if (_taskActiveNow(task, now)) {
        _currentTask  = &task;
        _currentGroup = &grp;
      } else {
        int startMin = task.startHour * 60 + task.startMin;
        int delta    = startMin - nowMin;
        if (delta > 0 && delta < nextDelta) {
          nextDelta = delta;
          _nextTask = &task;
        }
      }
    }
  }
}

void TaskScheduler::tick(struct tm* now) {
  bool wasActive = (_currentTask != nullptr);
  _findActiveTask(now);

  if (!_currentTask) {
    _elapsedSec = _remainingSec = 0;
    if (wasActive) {
      // Task just ended, reset fire flags for next task
      _firedStart = _firedMid = _firedOneMin = _firedDone = false;
    }
    return;
  }

  int nowSec   = now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec;
  int startSec = _currentTask->startHour * 3600 + _currentTask->startMin * 60;
  int durSec   = _currentTask->durationMin * 60;

  _elapsedSec   = nowSec - startSec;
  _remainingSec = durSec - _elapsedSec;

  // Reset fire flags when a new task becomes active
  static String lastTaskId = "";
  if (_currentTask->id != lastTaskId) {
    lastTaskId    = _currentTask->id;
    _firedStart   = false;
    _firedMid     = false;
    _firedOneMin  = false;
    _firedDone    = false;
  }

  if (!_firedStart && _elapsedSec >= 0) {
    if (_eventCb) _eventCb(*_currentTask, TaskEvent::START);
    _firedStart = true;
  }
  if (!_firedMid && _elapsedSec >= durSec / 2) {
    if (_eventCb) _eventCb(*_currentTask, TaskEvent::MIDPOINT);
    _firedMid = true;
  }
  if (!_firedOneMin && _remainingSec <= 60 && _remainingSec > 0) {
    if (_eventCb) _eventCb(*_currentTask, TaskEvent::ONE_MINUTE);
    _firedOneMin = true;
  }
  if (!_firedDone && _remainingSec <= 0) {
    if (_eventCb) _eventCb(*_currentTask, TaskEvent::COMPLETE);
    _firedDone = true;
  }
}

float TaskScheduler::progressPct() const {
  if (!_currentTask) return 0.0f;
  int dur = _currentTask->durationMin * 60;
  return dur > 0 ? (float)_elapsedSec / (float)dur * 100.0f : 0.0f;
}
```

---

### DisplayManager

**`src/display/DisplayManager.h`**
```cpp
#pragma once
#include <TFT_eSPI.h>
#include "../tasks/TaskScheduler.h"

struct WeatherData {
  float  temp       = 0;
  float  feelsLike  = 0;
  int    humidity   = 0;
  float  windSpeed  = 0;
  String description;
  String icon;
  bool   valid = false;
};

enum class Screen { HOME, TASK_LIST };

class DisplayManager {
public:
  static DisplayManager& instance();
  void begin();
  void update(struct tm* now);
  void setScreen(Screen s);

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute
  int pollTouch();

private:
  DisplayManager() {}

  TFT_eSPI    _tft;
  Screen      _screen = Screen::HOME;
  bool        _screenDirty = true;
  int         _scrollOffset = 0;    // For task list scrolling
  bool        _groupExpanded[16] = {};

  // Drawing methods
  void _drawStatusBar(struct tm* now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();

  // Widget helpers
  void _drawWifiArcs   (int cx, int cy, int rssi);
  void _drawBatteryIcon(int x,  int y,  int pct);
  void _drawProgressBar(int x,  int y,  int w, int h, float pct, uint32_t color);

  // Color palette (RGB565)
  static constexpr uint32_t CLR_BG       = 0x1082;
  static constexpr uint32_t CLR_CARD     = 0x2104;
  static constexpr uint32_t CLR_ACCENT   = 0x07FF;  // cyan
  static constexpr uint32_t CLR_TEXT     = 0xFFFF;
  static constexpr uint32_t CLR_SUBTEXT  = 0xAD75;  // gray
  static constexpr uint32_t CLR_GREEN    = 0x07E0;
  static constexpr uint32_t CLR_YELLOW   = 0xFFE0;
  static constexpr uint32_t CLR_RED      = 0xF800;
  static constexpr uint32_t CLR_BAR_BG   = 0x39C7;
  static constexpr uint32_t CLR_STATUSBG = 0x0841;
};
```

**`src/display/DisplayManager.cpp`**
```cpp
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
```

---

### WebServer

**`src/network/WebServer.h`**
```cpp
#pragma once
#include <ESPAsyncWebServer.h>

class AppWebServer {
public:
  static AppWebServer& instance();
  void begin();
  void broadcastStatus();  // Call from main loop every 1s

private:
  AppWebServer() : _server(80), _ws("/ws") {}
  AsyncWebServer _server;
  AsyncWebSocket _ws;

  void   _setupRoutes();
  void   _onWsEvent(AsyncWebSocket*, AsyncWebSocketClient*,
                    AwsEventType, void*, uint8_t*, size_t);
  String _buildStatusJson();
};
```

**`src/network/WebServer.cpp`**
```cpp
#include "WebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include "../config/Config.h"
#include "../tasks/TaskScheduler.h"
#include "../sensors/BatteryMonitor.h"
#include "../display/DisplayManager.h"

AppWebServer& AppWebServer::instance() {
  static AppWebServer inst;
  return inst;
}

void AppWebServer::begin() {
  // CORS headers for browser dev access
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  _ws.onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                     AwsEventType t, void* a, uint8_t* d, size_t l) {
    _onWsEvent(s, c, t, a, d, l);
  });

  _server.addHandler(&_ws);
  _setupRoutes();
  _server.begin();

  Serial.printf("[WebServer] Started at http://%s\n",
                WiFi.localIP().toString().c_str());
}

void AppWebServer::_setupRoutes() {
  // Static files from LittleFS
  _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ── GET /api/status ────────────────────────────────────────────────────────
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", _buildStatusJson());
  });

  // ── GET /api/schedule ──────────────────────────────────────────────────────
  _server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(LittleFS, "/schedule.json", "application/json");
  });

  // ── POST /api/schedule ─────────────────────────────────────────────────────
  _server.on("/api/schedule", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      static String buf;
      buf += String((char*)data).substring(0, len);
      if (index + len == total) {
        File f = LittleFS.open("/schedule.json", "w");
        f.print(buf);
        f.close();
        buf = "";
        TaskScheduler::instance().loadFromFile();
        req->send(200, "application/json", "{\"ok\":true}");
      }
    }
  );

  // ── GET /api/config ────────────────────────────────────────────────────────
  _server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    auto& cfg = Config::instance();
    JsonDocument doc;
    doc["timezone"] = cfg.data.timezone;
    doc["city"]     = cfg.data.city;
    doc["metric"]   = cfg.data.metricUnits;
    doc["muted"]    = cfg.data.muted;
    // Never expose owmApiKey over HTTP for basic security
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  // ── POST /api/config ───────────────────────────────────────────────────────
  _server.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      deserializeJson(doc, data, len);
      auto& cfg = Config::instance();
      if (doc["timezone"].is<String>()) {
        cfg.data.timezone = doc["timezone"].as<String>();
        setenv("TZ", cfg.data.timezone.c_str(), 1);
        tzset();
      }
      if (doc["city"].is<String>())   cfg.data.city        = doc["city"].as<String>();
      if (doc["owmkey"].is<String>()) cfg.data.owmApiKey   = doc["owmkey"].as<String>();
      if (doc["metric"].is<bool>())   cfg.data.metricUnits = doc["metric"].as<bool>();
      cfg.save();
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // ── POST /api/mute ─────────────────────────────────────────────────────────
  _server.on("/api/mute", HTTP_POST, [](AsyncWebServerRequest* req) {
    auto& cfg = Config::instance();
    cfg.data.muted = !cfg.data.muted;
    cfg.save();
    String resp = String("{\"muted\":") + (cfg.data.muted ? "true" : "false") + "}";
    req->send(200, "application/json", resp);
  });

  // ── POST /save-setup (captive portal) ─────────────────────────────────────
  _server.on("/save-setup", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
      JsonDocument doc;
      deserializeJson(doc, data, len);
      auto& cfg = Config::instance();
      cfg.data.wifiSSID     = doc["ssid"]     | "";
      cfg.data.wifiPassword = doc["password"] | "";
      cfg.data.timezone     = doc["timezone"] | "UTC0";
      cfg.data.city         = doc["city"]     | "London";
      cfg.data.owmApiKey    = doc["owmkey"]   | "";
      cfg.data.metricUnits  = doc["metric"]   | true;
      cfg.save();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved. Rebooting...\"}");
      delay(1000);
      ESP.restart();
    }
  );

  // Captive portal catch-all redirect
  _server.onNotFound([](AsyncWebServerRequest* req) {
    req->redirect("http://192.168.4.1/setup.html");
  });
}

String AppWebServer::_buildStatusJson() {
  auto& sched = TaskScheduler::instance();
  auto& bat   = BatteryMonitor::instance();
  auto& cfg   = Config::instance();
  auto& wx    = DisplayManager::instance().weather;

  time_t   now_t = time(nullptr);
  struct tm* now = localtime(&now_t);
  char timeBuf[9], dateBuf[24];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", now);
  strftime(dateBuf, sizeof(dateBuf), "%A %d %b %Y", now);

  JsonDocument doc;
  doc["type"]        = "status";
  doc["time"]        = timeBuf;
  doc["date"]        = dateBuf;
  doc["battery_pct"] = bat.percentage();
  doc["rssi"]        = WiFi.isConnected() ? (int)WiFi.RSSI() : 0;
  doc["muted"]       = cfg.data.muted;

  const Task* ct = sched.currentTask();
  if (ct) {
    doc["current_task"]  = ct->name;
    doc["elapsed_sec"]   = sched.elapsedSec();
    doc["remaining_sec"] = sched.remainingSec();
    doc["duration_sec"]  = ct->durationMin * 60;
    doc["progress_pct"]  = sched.progressPct();
    if (sched.currentGroup())
      doc["group"] = sched.currentGroup()->name;
  }
  if (sched.nextTask())
    doc["next_task"] = sched.nextTask()->name;

  if (wx.valid) {
    doc["weather"]["temp"]  = wx.temp;
    doc["weather"]["desc"]  = wx.description;
    doc["weather"]["icon"]  = wx.icon;
    doc["weather"]["humid"] = wx.humidity;
    doc["weather"]["wind"]  = wx.windSpeed;
  }

  String out;
  serializeJson(doc, out);
  return out;
}

void AppWebServer::broadcastStatus() {
  if (_ws.count() == 0) return;
  _ws.textAll(_buildStatusJson());

  // Periodic cleanup of closed connections
  static uint32_t lastCleanup = 0;
  if (millis() - lastCleanup > 5000) {
    lastCleanup = millis();
    _ws.cleanupClients();
  }
}

void AppWebServer::_onWsEvent(AsyncWebSocket* server,
                              AsyncWebSocketClient* client,
                              AwsEventType type,
                              void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected from %s\n",
                  client->id(), client->remoteIP().toString().c_str());
    // Push current status immediately on connect
    client->text(_buildStatusJson());

  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());

  } else if (type == WS_EVT_ERROR) {
    Serial.printf("[WS] Error from client #%u\n", client->id());

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    // Guard: only process complete single-frame text messages
    if (info->final && info->index == 0 &&
        info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      JsonDocument doc;
      if (deserializeJson(doc, (char*)data)) return;

      String cmd = doc["type"] | "";
      if (cmd == "mute_toggle") {
        auto& cfg = Config::instance();
        cfg.data.muted = !cfg.data.muted;
        cfg.save();
        server->textAll(_buildStatusJson());
      } else if (cmd == "reload_schedule") {
        TaskScheduler::instance().loadFromFile();
        server->textAll(_buildStatusJson());
      }
    }
  }
}
```

---

### AudioManager

Uses the native ESP-IDF `driver/i2s.h` — no third-party audio library required, no MIDI dependency, no RISC-V compile issues. Reads the WAV header on each file open to auto-configure sample rate and bit depth, then streams PCM data into the I2S DMA buffer in chunks from `loop()`.

**`src/audio/AudioManager.h`**
```cpp
#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <LittleFS.h>

class AudioManager {
public:
  static AudioManager& instance();
  void begin();
  void play(const char* path);  // path in LittleFS e.g. "/audio/starting.wav"
  void stop();
  void loop();                  // Call every loop() iteration
  bool isPlaying() const { return _playing; }

private:
  AudioManager() {}

  static constexpr i2s_port_t I2S_PORT  = I2S_NUM_0;
  static constexpr int         PIN_BCLK  = 20;
  static constexpr int         PIN_LRCLK = 21;
  static constexpr int         PIN_DOUT  = 2;
  static constexpr size_t      CHUNK     = 512;  // bytes per DMA write

  File   _file;
  bool   _playing = false;

  void _installDriver();
  void _uninstallDriver();
  bool _consumeWavHeader();     // Reads 44-byte header, reconfigures I2S clock
};
```

**`src/audio/AudioManager.cpp`**
```cpp
#include "AudioManager.h"

AudioManager& AudioManager::instance() {
  static AudioManager inst;
  return inst;
}

void AudioManager::_installDriver() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = 8000,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
  };

  i2s_pin_config_t pins = {
    .bck_io_num   = PIN_BCLK,
    .ws_io_num    = PIN_LRCLK,
    .data_out_num = PIN_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void AudioManager::_uninstallDriver() {
  i2s_driver_uninstall(I2S_PORT);
}

void AudioManager::begin() {
  _installDriver();
  Serial.println("[AudioManager] I2S ready");
}

// Reads the standard 44-byte WAV header, validates RIFF/WAVE markers,
// then reconfigures the I2S clock to match the file's actual sample rate
// and bit depth. This means you can mix 8kHz and 16kHz clips freely.
bool AudioManager::_consumeWavHeader() {
  uint8_t header[44];
  if (_file.read(header, 44) != 44) return false;

  // Validate RIFF....WAVE marker
  if (header[0] != 'R' || header[1] != 'I' ||
      header[2] != 'F' || header[3] != 'F') return false;
  if (header[8]  != 'W' || header[9]  != 'A' ||
      header[10] != 'V' || header[11] != 'E') return false;

  // Sample rate: bytes 24–27 little-endian
  uint32_t sampleRate = header[24] | (header[25] << 8) |
                        (header[26] << 16) | (header[27] << 24);

  // Bits per sample: bytes 34–35 little-endian
  uint16_t bitsPerSample = header[34] | (header[35] << 8);

  Serial.printf("[AudioManager] WAV: %uHz %u-bit\n", sampleRate, bitsPerSample);

  i2s_set_clk(I2S_PORT,
               sampleRate,
               (i2s_bits_per_sample_t)bitsPerSample,
               I2S_CHANNEL_MONO);
  return true;
}

void AudioManager::play(const char* path) {
  stop();

  _file = LittleFS.open(path, "r");
  if (!_file) {
    Serial.printf("[AudioManager] Not found: %s\n", path);
    return;
  }

  if (!_consumeWavHeader()) {
    Serial.printf("[AudioManager] Bad WAV header: %s\n", path);
    _file.close();
    return;
  }

  _playing = true;
  Serial.printf("[AudioManager] Playing: %s\n", path);
}

void AudioManager::stop() {
  if (_file) _file.close();
  _playing = false;
  i2s_zero_dma_buffer(I2S_PORT);
}

// Called every loop() — streams the next CHUNK of PCM data into the I2S
// DMA buffer. Non-blocking: i2s_write returns immediately if buffer is full.
void AudioManager::loop() {
  if (!_playing || !_file) return;

  uint8_t buf[CHUNK];
  size_t  bytesRead = _file.read(buf, CHUNK);

  if (bytesRead == 0) {
    stop();  // EOF
    return;
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_PORT, buf, bytesRead, &bytesWritten, portMAX_DELAY);
}
```

> **Why no ESP8266Audio?** The `earlephilhower/ESP8266Audio` library includes `AudioGeneratorMIDI` which has compile-time dependencies that fail on the ESP32-C3's RISC-V core. Removing it entirely and using `driver/i2s.h` directly avoids the issue — the I2S driver ships with the `espressif32` platform and needs no extra `lib_deps` entry.
>
> **Audio file format:** Standard WAV, any sample rate. The WAV header is read on each `play()` call and the I2S clock is reconfigured automatically. Recommended format for voice prompts:
> ```bash
> ffmpeg -i input.mp3 -ar 8000 -ac 1 -acodec pcm_s16le data/audio/starting.wav
> ```
> At 8kHz mono 16-bit, a 3-second clip is ~48KB. LittleFS on the XIAO C3 gives ~1MB usable — enough for 15–20 clips alongside the HTML files.

---

### BatteryMonitor

**`src/sensors/BatteryMonitor.h`**
```cpp
#pragma once
#include <Arduino.h>

class BatteryMonitor {
public:
  static BatteryMonitor& instance();
  void begin(int adcPin);
  void update();            // Call periodically (every 30s is fine)
  int  percentage() const { return _pct; }
  float voltage()   const { return _voltage; }

private:
  BatteryMonitor() {}
  int   _adcPin  = -1;
  int   _pct     = 0;
  float _voltage = 0.0f;

  // Voltage divider: 100kΩ + 100kΩ, ratio = 0.5
  // ESP32-C3 ADC reference = 2500mV (attenuation DB_11 for 0–3.1V range)
  static constexpr float DIVIDER_RATIO  = 0.5f;
  static constexpr float ADC_MAX_MV     = 2500.0f;
  static constexpr int   ADC_RESOLUTION = 4095;

  // LiPo voltage thresholds
  static constexpr float VBAT_FULL  = 4.20f;
  static constexpr float VBAT_EMPTY = 3.00f;

  float _readVoltage();
};
```

**`src/sensors/BatteryMonitor.cpp`**
```cpp
#include "BatteryMonitor.h"

BatteryMonitor& BatteryMonitor::instance() {
  static BatteryMonitor inst;
  return inst;
}

void BatteryMonitor::begin(int adcPin) {
  _adcPin = adcPin;
  analogSetAttenuation(ADC_11db);  // 0–3.1V range
  // Initial read
  update();
}

float BatteryMonitor::_readVoltage() {
  // Average 16 samples to reduce ADC noise
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(_adcPin);
    delayMicroseconds(100);
  }
  float adcAvg  = sum / 16.0f;
  float adcMv   = (adcAvg / ADC_RESOLUTION) * ADC_MAX_MV;
  float batMv   = adcMv / DIVIDER_RATIO;
  return batMv / 1000.0f;  // Return in Volts
}

void BatteryMonitor::update() {
  _voltage = _readVoltage();
  float clamped = constrain(_voltage, VBAT_EMPTY, VBAT_FULL);
  _pct = (int)((clamped - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0f);

  Serial.printf("[Battery] %.2fV → %d%%\n", _voltage, _pct);
}
```

> **ADC accuracy note:** The ESP32-C3's ADC has a non-linear response, especially near the edges of its range. For better accuracy, apply Espressif's ADC calibration (`esp_adc_cal_characterize()`) from the ESP-IDF ADC calibration component, or add a lookup-table correction. For a schedule tracker, ±5% accuracy from the simple linear approach is acceptable.

---

### `main.cpp`

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config/Config.h"
#include "display/DisplayManager.h"
#include "network/WebServer.h"
#include "tasks/TaskScheduler.h"
#include "audio/AudioManager.h"
#include "sensors/BatteryMonitor.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
DNSServer dnsServer;
bool      apMode = false;

uint32_t lastTick       = 0;
uint32_t lastWxFetch    = 0;
uint32_t lastWsBcast    = 0;
uint32_t lastBatUpdate  = 0;

// ─── Weather ─────────────────────────────────────────────────────────────────
void fetchWeather() {
  auto& cfg = Config::instance();
  auto& wx  = DisplayManager::instance().weather;

  if (cfg.data.owmApiKey.isEmpty() || cfg.data.city.isEmpty()) return;

  HTTPClient http;
  String units = cfg.data.metricUnits ? "metric" : "imperial";
  String url   = "http://api.openweathermap.org/data/2.5/weather?q=" +
                 cfg.data.city + "&units=" + units +
                 "&appid=" + cfg.data.owmApiKey;

  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();

  if (code == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (!err) {
      wx.temp        = doc["main"]["temp"].as<float>();
      wx.feelsLike   = doc["main"]["feels_like"].as<float>();
      wx.humidity    = doc["main"]["humidity"].as<int>();
      wx.windSpeed   = doc["wind"]["speed"].as<float>();
      wx.description = doc["weather"][0]["description"].as<String>();
      wx.icon        = doc["weather"][0]["icon"].as<String>();
      wx.valid       = true;
      Serial.printf("[Weather] %.1f°  %s\n", wx.temp, wx.description.c_str());
    }
  } else {
    Serial.printf("[Weather] HTTP error: %d\n", code);
  }
  http.end();
}

// ─── WiFi STA mode ───────────────────────────────────────────────────────────
void startSTA() {
  auto& cfg = Config::instance();
  Serial.printf("[WiFi] Connecting to: %s\n", cfg.data.wifiSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.data.wifiSSID.c_str(), cfg.data.wifiPassword.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Failed to connect — falling back to AP mode");
    startAP();  // Declare before use; see below
    return;
  }

  Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // Timezone + NTP
  setenv("TZ", cfg.data.timezone.c_str(), 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  // Wait for valid time sync (up to 10s)
  struct tm ti;
  int attempts = 0;
  while (!getLocalTime(&ti) && attempts++ < 20) {
    delay(500);
  }
  if (attempts < 20) {
    Serial.println("[NTP] Time synced");
  } else {
    Serial.println("[NTP] Sync timeout — using RTC if available");
  }

  fetchWeather();
  lastWxFetch = millis();
}

// ─── WiFi AP mode (captive portal) ───────────────────────────────────────────
void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ScheduleTracker-Setup", "setup1234");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  Serial.println("[WiFi] AP mode: ScheduleTracker-Setup");
  Serial.println("[WiFi] Portal: http://192.168.4.1");
}

// ─── Task event handler ───────────────────────────────────────────────────────
void onTaskEvent(const Task& task, TaskEvent event) {
  if (Config::instance().data.muted) return;

  switch (event) {
    case TaskEvent::START:
      Serial.printf("[Task] START: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioStart.isEmpty() ? "/audio/starting.wav" : task.audioStart.c_str());
      break;
    case TaskEvent::MIDPOINT:
      Serial.printf("[Task] MIDPOINT: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioMid.isEmpty() ? "/audio/halfway.wav" : task.audioMid.c_str());
      break;
    case TaskEvent::ONE_MINUTE:
      Serial.printf("[Task] ONE_MINUTE: %s\n", task.name.c_str());
      AudioManager::instance().play("/audio/onemin.wav");
      break;
    case TaskEvent::COMPLETE:
      Serial.printf("[Task] COMPLETE: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioDone.isEmpty() ? "/audio/done.wav" : task.audioDone.c_str());
      break;
  }
}

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[Boot] Schedule Tracker starting...");

  // Init LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount failed — formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  // Load config
  Config::instance().load();

  // Init hardware
  BatteryMonitor::instance().begin(/*adcPin=*/1);
  DisplayManager::instance().begin();
  AudioManager::instance().begin();

  // Load schedule
  TaskScheduler::instance().loadFromFile();
  TaskScheduler::instance().onEvent(onTaskEvent);

  // Network
  if (Config::instance().data.wifiSSID.isEmpty()) {
    startAP();
  } else {
    startSTA();
  }

  // Web server (serves both AP portal and STA dashboard)
  AppWebServer::instance().begin();

  Serial.println("[Boot] Ready.");
}

// ─── loop() ──────────────────────────────────────────────────────────────────
void loop() {
  // DNS for captive portal
  if (apMode) dnsServer.processNextRequest();

  uint32_t now_ms = millis();

  // ── 1-second tick ──────────────────────────────────────────────────────────
  if (now_ms - lastTick >= 1000) {
    lastTick = now_ms;

    time_t   t   = time(nullptr);
    struct tm* tm_now = localtime(&t);

    TaskScheduler::instance().tick(tm_now);
    DisplayManager::instance().update(tm_now);
  }

  // ── WebSocket broadcast every 1s ───────────────────────────────────────────
  if (now_ms - lastWsBcast >= 1000) {
    lastWsBcast = now_ms;
    AppWebServer::instance().broadcastStatus();
  }

  // ── Battery update every 30s ───────────────────────────────────────────────
  if (now_ms - lastBatUpdate >= 30000) {
    lastBatUpdate = now_ms;
    BatteryMonitor::instance().update();
  }

  // ── Weather refresh every 20 minutes ───────────────────────────────────────
  if (!apMode && now_ms - lastWxFetch >= 20UL * 60 * 1000) {
    lastWxFetch = now_ms;
    fetchWeather();
  }

  // ── Touch input ────────────────────────────────────────────────────────────
  static uint32_t lastTouch = 0;
  if (now_ms - lastTouch > 300) {   // 300ms debounce
    int zone = DisplayManager::instance().pollTouch();
    if (zone >= 0) {
      lastTouch = now_ms;
      switch (zone) {
        case 1:  // All Tasks
          DisplayManager::instance().setScreen(Screen::TASK_LIST);
          break;
        case 2:  // Mute toggle
          {
            auto& cfg = Config::instance();
            cfg.data.muted = !cfg.data.muted;
            cfg.save();
            Serial.printf("[Touch] Mute: %s\n", cfg.data.muted ? "ON" : "OFF");
          }
          break;
        case 3:  // Back (from task list)
          DisplayManager::instance().setScreen(Screen::HOME);
          break;
      }
    }
  }

  // ── Audio ──────────────────────────────────────────────────────────────────
  AudioManager::instance().loop();
}
```

---

## Web Portal

### `data/index.html` — Live Dashboard (WebSocket)

```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Schedule Tracker</title>
<style>
  :root {
    --bg: #0d1117; --card: #161b22; --accent: #00bfff;
    --text: #e6edf3; --sub: #8b949e; --green: #3fb950;
    --yellow: #d29922; --red: #f85149; --bar-bg: #21262d;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: var(--bg); color: var(--text);
         font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
         max-width: 480px; margin: 0 auto; padding: 16px; }

  .status-bar {
    display: flex; justify-content: space-between; align-items: center;
    padding: 8px 12px; background: var(--card);
    border-radius: 12px; margin-bottom: 12px; font-size: 13px; color: var(--sub);
  }
  .wifi-arcs { display: flex; align-items: flex-end; gap: 3px; height: 16px; }
  .wifi-arcs span {
    display: block; width: 4px; border-radius: 2px; background: var(--bar-bg);
    transition: background 0.4s;
  }
  .wifi-arcs span.active { background: var(--accent); }
  .wifi-arcs span:nth-child(1) { height: 4px; }
  .wifi-arcs span:nth-child(2) { height: 7px; }
  .wifi-arcs span:nth-child(3) { height: 10px; }
  .wifi-arcs span:nth-child(4) { height: 14px; }

  .battery { display: flex; align-items: center; gap: 6px; }
  .battery-icon {
    width: 28px; height: 14px; border: 2px solid var(--sub);
    border-radius: 3px; position: relative; padding: 2px;
  }
  .battery-icon::after {
    content: ''; position: absolute; right: -5px; top: 3px;
    width: 3px; height: 6px; background: var(--sub); border-radius: 0 2px 2px 0;
  }
  .battery-fill {
    height: 100%; border-radius: 1px; transition: width 0.5s, background 0.5s;
  }

  .weather-card {
    background: var(--card); border-radius: 12px;
    padding: 14px 16px; margin-bottom: 12px;
    display: flex; align-items: center; justify-content: space-between;
  }
  .weather-temp { font-size: 2rem; font-weight: 600; color: var(--text); }
  .weather-details { font-size: 12px; color: var(--sub); text-align: right; }

  .task-card {
    background: var(--card); border-radius: 12px;
    padding: 18px 16px; margin-bottom: 12px;
  }
  .task-group { font-size: 11px; color: var(--sub); text-transform: uppercase;
                letter-spacing: 0.08em; margin-bottom: 4px; }
  .task-name  { font-size: 1.5rem; font-weight: 600; margin-bottom: 14px; }
  .task-meta  { display: flex; justify-content: space-between;
                font-size: 13px; color: var(--sub); margin-bottom: 8px; }
  .task-remaining { font-size: 1.25rem; font-weight: 600; color: var(--accent); }

  .progress-bar {
    height: 8px; background: var(--bar-bg);
    border-radius: 4px; overflow: hidden; margin-bottom: 6px;
  }
  .progress-fill {
    height: 100%; border-radius: 4px; background: var(--accent);
    transition: width 0.8s ease;
  }

  .next-task {
    background: var(--bar-bg); border-radius: 8px;
    padding: 10px 12px; font-size: 13px; color: var(--sub); margin-top: 8px;
  }
  .next-task strong { color: var(--text); }

  .controls {
    display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 12px;
  }
  .btn {
    padding: 14px; border-radius: 10px; border: 1px solid var(--bar-bg);
    background: var(--card); color: var(--text); font-size: 14px;
    cursor: pointer; transition: background 0.2s, border-color 0.2s; text-align: center;
  }
  .btn:hover { background: var(--bar-bg); }
  .btn.active { border-color: var(--red); color: var(--red); }
  .btn-sub { font-size: 11px; color: var(--sub); margin-top: 3px; }

  .conn-dot {
    display: inline-block; width: 8px; height: 8px;
    border-radius: 50%; margin-right: 6px;
    background: var(--red); transition: background 0.3s;
  }
  .conn-dot.connected { background: var(--green); }

  a { color: var(--accent); text-decoration: none; font-size: 13px; }
</style>
</head>
<body>

<div class="status-bar">
  <div style="display:flex;align-items:center;gap:10px">
    <div class="wifi-arcs" id="wifiArcs">
      <span></span><span></span><span></span><span></span>
    </div>
    <span id="rssiVal" style="font-size:11px"></span>
  </div>
  <div id="timeDisplay" style="font-size:15px;font-weight:600;color:var(--text)">--:--</div>
  <div class="battery">
    <div class="battery-icon">
      <div class="battery-fill" id="batFill" style="width:0%;background:var(--green)"></div>
    </div>
    <span id="batPct">--%</span>
  </div>
</div>

<div class="weather-card" id="weatherCard" style="display:none">
  <div>
    <div class="weather-temp" id="wxTemp">--°</div>
    <div style="font-size:13px;color:var(--sub)" id="wxDesc">--</div>
  </div>
  <div class="weather-details">
    <div id="wxHumid">💧 --%</div>
    <div id="wxWind">💨 -- km/h</div>
  </div>
</div>

<div class="task-card" id="taskCard">
  <div class="task-group" id="taskGroup">—</div>
  <div class="task-name" id="taskName">No active task</div>
  <div class="task-meta">
    <span id="taskProgress">—</span>
    <span class="task-remaining" id="taskRemaining">—</span>
  </div>
  <div class="progress-bar">
    <div class="progress-fill" id="progressFill" style="width:0%"></div>
  </div>
  <div class="next-task" id="nextTask" style="display:none">
    Next: <strong id="nextTaskName">—</strong>
  </div>
</div>

<div class="controls">
  <div class="btn" id="muteBtn" onclick="sendCmd('mute_toggle')">
    🔔 Mute
    <div class="btn-sub" id="muteSub">Audio on</div>
  </div>
  <a href="/config.html" class="btn" style="display:block">
    ⚙️ Config
    <div class="btn-sub">Tasks & settings</div>
  </a>
</div>

<div style="text-align:center;padding:8px 0">
  <span class="conn-dot" id="connDot"></span>
  <span id="connStatus" style="font-size:12px;color:var(--sub)">Connecting…</span>
</div>

<script>
let ws, reconnectTimer;

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);

  ws.onopen = () => {
    document.getElementById('connDot').classList.add('connected');
    document.getElementById('connStatus').textContent = 'Live';
    clearTimeout(reconnectTimer);
  };

  ws.onclose = () => {
    document.getElementById('connDot').classList.remove('connected');
    document.getElementById('connStatus').textContent = 'Reconnecting…';
    reconnectTimer = setTimeout(connect, 3000);
  };

  ws.onmessage = (e) => {
    try { updateUI(JSON.parse(e.data)); } catch(err) {}
  };
}

function sendCmd(type) {
  if (ws && ws.readyState === WebSocket.OPEN)
    ws.send(JSON.stringify({ type }));
}

function fmtTime(sec) {
  const m = Math.floor(sec / 60), s = sec % 60;
  return `${m}:${s.toString().padStart(2,'0')}`;
}

function updateWifiArcs(rssi) {
  const bars = rssi === 0 ? 0 : rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
  document.querySelectorAll('#wifiArcs span').forEach((el, i) => {
    el.classList.toggle('active', i < bars);
  });
  document.getElementById('rssiVal').textContent = rssi ? `${rssi}dBm` : 'Off';
}

function updateBattery(pct) {
  const fill   = document.getElementById('batFill');
  const pctEl  = document.getElementById('batPct');
  pctEl.textContent = `${pct}%`;
  fill.style.width  = `${pct}%`;
  fill.style.background = pct > 50 ? 'var(--green)' : pct > 20 ? 'var(--yellow)' : 'var(--red)';
}

function updateUI(d) {
  // Time
  document.getElementById('timeDisplay').textContent = d.time ? d.time.substring(0,5) : '--:--';

  // WiFi + battery
  updateWifiArcs(d.rssi || 0);
  updateBattery(d.battery_pct || 0);

  // Weather
  if (d.weather) {
    document.getElementById('weatherCard').style.display = 'flex';
    document.getElementById('wxTemp').textContent  = `${Math.round(d.weather.temp)}°`;
    document.getElementById('wxDesc').textContent  = d.weather.desc || '';
    document.getElementById('wxHumid').textContent = `💧 ${d.weather.humid}%`;
    document.getElementById('wxWind').textContent  = `💨 ${Math.round(d.weather.wind)} km/h`;
  }

  // Current task
  if (d.current_task) {
    document.getElementById('taskGroup').textContent     = d.group || '';
    document.getElementById('taskName').textContent      = d.current_task;
    document.getElementById('taskRemaining').textContent = fmtTime(d.remaining_sec);
    document.getElementById('taskProgress').textContent  = `${Math.round(d.progress_pct)}% complete`;
    document.getElementById('progressFill').style.width  = `${d.progress_pct}%`;
  } else {
    document.getElementById('taskName').textContent      = 'No active task';
    document.getElementById('taskGroup').textContent     = '';
    document.getElementById('taskRemaining').textContent = '';
    document.getElementById('progressFill').style.width  = '0%';
  }

  // Next task
  const nextEl = document.getElementById('nextTask');
  if (d.next_task) {
    nextEl.style.display = 'block';
    document.getElementById('nextTaskName').textContent = d.next_task;
  } else {
    nextEl.style.display = 'none';
  }

  // Mute button
  const muteBtn = document.getElementById('muteBtn');
  const muteSub = document.getElementById('muteSub');
  muteBtn.classList.toggle('active', d.muted);
  muteBtn.innerHTML = d.muted ? '🔇 Unmute' : '🔔 Mute';
  muteBtn.innerHTML += `<div class="btn-sub">${d.muted ? 'Audio off' : 'Audio on'}</div>`;
}

connect();
</script>
</body>
</html>
```

---

## Schedule JSON Format

**`data/schedule.json`**
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
        },
        {
          "id": "t2",
          "name": "Drink Coffee",
          "start_time": "06:10",
          "duration_min": 5,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "daily",
          "enabled":     true
        },
        {
          "id": "t3",
          "name": "Wash Up",
          "start_time": "06:15",
          "duration_min": 15,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "weekdays",
          "enabled":     true
        },
        {
          "id": "t4",
          "name": "Get Dressed",
          "start_time": "06:30",
          "duration_min": 10,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "weekdays",
          "enabled":     true
        },
        {
          "id": "t5",
          "name": "Breakfast",
          "start_time": "06:40",
          "duration_min": 20,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "daily",
          "enabled":     true
        }
      ]
    },
    {
      "id": "workblock",
      "name": "Work Block",
      "icon": "briefcase",
      "color": "#4A9EFF",
      "tasks": [
        {
          "id": "t6",
          "name": "Deep Work",
          "start_time": "09:00",
          "duration_min": 90,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "weekdays",
          "enabled":     true
        },
        {
          "id": "t7",
          "name": "Stand-up Meeting",
          "start_time": "10:30",
          "duration_min": 15,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "weekdays",
          "enabled":     true
        }
      ]
    },
    {
      "id": "winddown",
      "name": "Wind Down",
      "icon": "moon",
      "color": "#A78BFA",
      "tasks": [
        {
          "id": "t8",
          "name": "Reading",
          "start_time": "21:00",
          "duration_min": 30,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "daily",
          "enabled":     true
        },
        {
          "id": "t9",
          "name": "Lights Out",
          "start_time": "21:30",
          "duration_min": 5,
          "audio_start": "",
          "audio_mid":   "",
          "audio_done":  "",
          "recurrence":  "daily",
          "enabled":     true
        }
      ]
    }
  ]
}
```

### Recurrence Options

| Value | Description |
|-------|-------------|
| `"daily"` | Every day |
| `"weekdays"` | Monday–Friday only |
| `"weekends"` | Saturday–Sunday only |

### Timezone POSIX Strings (Common)

| Location | POSIX String |
|----------|-------------|
| UTC | `UTC0` |
| London (GMT/BST) | `GMT0BST,M3.5.0/1,M10.5.0` |
| New York (ET) | `EST5EDT,M3.2.0,M11.1.0` |
| Chicago (CT) | `CST6CDT,M3.2.0,M11.1.0` |
| Denver (MT) | `MST7MDT,M3.2.0,M11.1.0` |
| Los Angeles (PT) | `PST8PDT,M3.2.0,M11.1.0` |
| Sydney (AEST) | `AEST-10AEDT,M10.1.0,M4.1.0/3` |
| Tokyo (JST) | `JST-9` |
| Berlin (CET) | `CET-1CEST,M3.5.0,M10.5.0/3` |
| Dubai (GST) | `GST-4` |
| India (IST) | `IST-5:30` |

---

## Audio File Preparation

### Using ffmpeg (required)

All audio files must be standard WAV — PCM, any sample rate. 8kHz mono 16-bit is recommended to keep file sizes small.

```bash
# Convert any audio file to device-compatible WAV
ffmpeg -i input.mp3 -ar 8000 -ac 1 -acodec pcm_s16le data/audio/starting.wav

# Verify the output
ffprobe data/audio/starting.wav
# Should show: Audio: pcm_s16le, 8000 Hz, mono, s16
```

### Generating Voice Prompts with gTTS

```python
# generate_audio.py — run once on your PC, outputs directly into data/audio/
from gtts import gTTS
import subprocess, os

prompts = {
    "starting": "Starting your task now.",
    "halfway":  "You are halfway through.",
    "onemin":   "One minute remaining.",
    "done":     "Task complete. Well done.",
}

os.makedirs("data/audio", exist_ok=True)

for name, text in prompts.items():
    mp3_path = f"/tmp/{name}.mp3"
    wav_path = f"data/audio/{name}.wav"
    gTTS(text=text, lang='en').save(mp3_path)
    subprocess.run([
        "ffmpeg", "-y", "-i", mp3_path,
        "-ar", "8000", "-ac", "1", "-acodec", "pcm_s16le",
        wav_path
    ])
    print(f"Generated: {wav_path}")
    os.remove(mp3_path)
```

Install dependencies: `pip install gtts`

### Volume Control

Volume is set in hardware via the MAX98357A GAIN pin. The native I2S driver has no software gain stage.

| GAIN pin wiring | Gain | Max power into 4Ω @ 3.7V | Use for |
|-----------------|------|--------------------------|---------|
| Float (default) | 9dB | ~1.5W | Good all-round starting point |
| GND directly | 6dB | ~0.75W | Quiet environments |
| 100kΩ to VDD | 12dB | ~3W | Loud rooms; monitor amp heat |

For a 3W 4Ω speaker, floating GAIN (9dB default) is the right balance — loud enough for a noisy room, well within the speaker's 3W rating from a 3.7V LiPo supply.

### Expected File Sizes (8kHz mono 16-bit WAV)

| Duration | File size |
|----------|-----------|
| 2 seconds | ~32 KB |
| 3 seconds | ~48 KB |
| 5 seconds | ~80 KB |

LittleFS on the XIAO ESP32-C3 provides ~1MB usable — enough for 15–20 voice clips alongside the HTML portal files.

---

## First Boot & Setup Flow

```
1. Power on device
   └─► LittleFS mounts, config loads — WiFi SSID is empty

2. Device broadcasts: "ScheduleTracker-Setup" (password: setup1234)

3. Connect phone/laptop to that network
   └─► Browser auto-redirects (captive portal) to http://192.168.4.1
       OR manually navigate to http://192.168.4.1/setup.html

4. Fill in setup form:
   ├─ WiFi network name (SSID)
   ├─ WiFi password
   ├─ Timezone (dropdown)
   ├─ City for weather (e.g. "London")
   ├─ OpenWeatherMap API key (free at openweathermap.org)
   └─ Units (°C/°F)

5. Tap Save — device reboots into STA mode

6. Device connects to home WiFi, syncs time via NTP, fetches weather

7. Access dashboard at http://[device-ip]
   └─► WebSocket live status updates begin immediately

8. Edit schedule at http://[device-ip]/config.html
```

### Getting an OpenWeatherMap API Key

1. Register free at [openweathermap.org](https://openweathermap.org/api)
2. Go to **API Keys** in your account
3. Copy the default key (active within a few minutes)
4. Free tier: 60 calls/minute, plenty for this project

### Factory Reset

Hold the reset button for 10 seconds, or from the serial monitor:
```
Config::instance().reset();
ESP.restart();
```

---

## Development Phases

| Phase | Focus | Est. Time |
|-------|-------|-----------|
| **Phase 1 — Bring-up** | TFT renders, touch responds, WiFi connects, NTP syncs, battery ADC reads | Week 1 |
| **Phase 2 — Task engine** | LittleFS JSON load/save, task ticker, audio cues with buzzer, web portal CRUD | Week 2 |
| **Phase 3 — Polish** | Weather API, scrollable task list, I2S audio with voice clips, mute logic, WebSocket dashboard | Week 3 |
| **Phase 4 — Hardware final** | Protoboard or PCB layout, case v1 print, fit-check, v2 print with tolerance fixes | Week 4 |

---

## Known Limitations & Future Ideas

- **ADC accuracy:** ESP32-C3 ADC is non-linear; consider the MAX17048 LiPo fuel gauge ($8) for coulomb-counting if battery % accuracy matters
- **Dynamic audio:** Task names use generic prompts; for name-specific voice ("Starting: Drink Coffee"), implement clip concatenation or add a small TTS cache
- **OTA updates:** Add ArduinoOTA for wireless firmware updates — saves repeated USB connections once the case is assembled
- **DST edge cases:** POSIX tz strings handle DST automatically via `tzset()` but verify your timezone string at [timezone.ctz.io](https://timezone.ctz.io)
- **Multi-day tasks:** Current scheduler is wall-clock daily only; overnight tasks (spanning midnight) are not supported in v1
- **Touch calibration:** XPT2046 may need calibration constants adjusted per panel; use TFT_eSPI's `touch_calibrate` example sketch first run

---

## License

MIT — build, mod, and share freely.
