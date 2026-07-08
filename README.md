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
   - [ClothingAdvisor](#clothingadvisor)
   - [BacklightManager](#backlightmanager)
   - [WebServer](#webserver)
   - [AudioManager](#audiomanager)
   - [BatteryMonitor](#batterymonitor)
   - [main.cpp](#maincpp)
9. [Screen Mockups](#screen-mockups)
10. [Web Portal](#web-portal)
    - [index.html — Live Dashboard](#data-indexhtml--live-dashboard-websocket)
    - [Setup Portal Validation](#setup-portal-validation-datasetuphtml)
    - [OWM Key Management](#owm-key-management-dataconfightml)
11. [Schedule JSON Format](#schedule-json-format)
12. [Audio File Preparation](#audio-file-preparation)
13. [First Boot & Setup Flow](#first-boot--setup-flow)
14. [Development Phases](#development-phases)

---

## Features

- **2.8" ILI9341 touch TFT** — 240×320 portrait display with XPT2046 resistive touch
- **Android-style status bar** — WiFi arc signal strength, battery percentage, time, date
- **Live weather** — OpenWeatherMap current conditions with offline cache — survives reboots and WiFi outages
- **Clothing recommendations** — Tap the weather bar for a "What to Wear" overlay based on temp, wind, humidity, and conditions
- **Grouped task scheduling** — Tasks organised into named groups (e.g. *Start of Day*, *Work Block*)
- **Dial timer with emoji** — A 270° arc dial shows time remaining; a mood emoji (😊 → 😰 → 😱) bobs inside the dial and changes expression as urgency rises; arc colour transitions cyan → yellow → red
- **Voice audio prompts** — I2S MAX98357A amplifier plays cues at task start, halfway, 1-minute warning, and completion
- **Touch beep feedback** — Short I2S-generated tone on every touch event (no WAV file needed)
- **Auto-dim backlight** — PWM-dimmed ILI9341 backlight fades after 60s of inactivity; wakes on touch
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
GPIO 7               TFT Backlight     TFT BL  (PWM via LEDC — auto-dim)

GPIO 20 (I2S BCLK)   I2S Bit Clock     MAX98357A BCLK
GPIO 21 (I2S LRCK)   I2S Word Select   MAX98357A LRCLK
GPIO 2  (I2S DOUT)   I2S Data Out      MAX98357A DIN

GPIO 1  (ADC1_0)     Battery ADC       Voltage divider midpoint

3V3                  Power             TFT VCC, Touch VCC, MAX98357A VDD
GND                  Ground            All grounds
5V (via 5V pin)      Charge input      TP4056 OUT+ → XIAO 5V pin
```

> **Note:** Touch IRQ (XPT2046 INT pin) is no longer assigned — polling via `getTouch()` is sufficient at 300ms debounce. GPIO 7 is reassigned from Touch IRQ to backlight PWM. If you want interrupt-driven touch, use a free GPIO and call `BacklightManager::instance().wake()` from the ISR.

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
├── screens.html                   ← Interactive screen mockups (open in browser)
├── data/                          ← Uploaded to LittleFS
│   ├── index.html                 ← Live dashboard (WebSocket client)
│   ├── config.html                ← Schedule + device config editor
│   ├── setup.html                 ← Captive portal first-run page
│   ├── schedule.json              ← Task data (editable via portal)
│   └── audio/
│       ├── starting.wav           ← 8kHz 16-bit mono WAV
│       ├── halfway.wav
│       ├── onemin.wav
│       └── done.wav
└── src/
    ├── main.cpp
    ├── config/
    │   ├── Config.h
    │   └── Config.cpp
    ├── display/
    │   ├── DisplayManager.h
    │   ├── DisplayManager.cpp
    │   ├── ClothingAdvisor.h      ← Weather-based clothing recommendation engine
    │   ├── ClothingAdvisor.cpp
    │   ├── BacklightManager.h     ← PWM auto-dim + wake on touch
    │   └── BacklightManager.cpp
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
#include <vector>
#include "../tasks/TaskScheduler.h"

struct WeatherData {
  float  temp       = 0;
  float  feelsLike  = 0;
  int    humidity   = 0;
  float  windSpeed  = 0;
  String description;
  String icon;
  String errorMsg;   // Set on fetch failure: "API key invalid", "City not found", etc.
  bool   valid = false;
};

enum class Screen { HOME, TASK_LIST };

class DisplayManager {
public:
  static DisplayManager& instance();
  void begin();
  void update(struct tm* now);
  void setScreen(Screen s);
  void showClothingOverlay();   // Blocks until tap or 8s timeout, then redraws

  WeatherData weather;

  // Returns touch zone: -1=none, 0=main, 1=allTasks, 2=mute, 3=back, 4=weather
  int pollTouch();

private:
  DisplayManager() {}

  TFT_eSPI    _tft;
  Screen      _screen = Screen::HOME;
  bool        _screenDirty = true;
  int         _scrollOffset = 0;    // For task list scrolling
  bool        _groupExpanded[16] = {};
  float       _gaugeAnimT = 0.0f;   // Seconds elapsed — drives gauge bob and arc pulse

  // Drawing methods
  void _drawStatusBar(struct tm* now);
  void _drawWeatherRow();
  void _drawCurrentTask();
  void _drawNavBar();
  void _drawTaskList();
  void _drawClothingOverlay(const std::vector<struct ClothingItem>& items);

  // Bunny timeline animation — drawn above the progress bar track
  // barX/barY/barW: progress bar geometry
  // progressPct: 0–100
  // remainingSec/totalSec: for speed scaling
  void _drawBunny(int barX, int barY, int barW,
                  float progressPct, int remainingSec, int totalSec);

  // Dial timer — 270° arc ring with emoji inside
  // cx=120, cy=174, r=64 for the 240×320 layout
  void _drawDial(int cx, int cy, int r,
                 float progressPct, int remainingSec, int totalSec,
                 float urgency, uint32_t arcColor);

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
// Shows error messages for specific failure modes (bad key, bad city, etc.)
// so the user knows to visit /config rather than just seeing a blank row.
// ─────────────────────────────────────────────────────────────────────────────
void DisplayManager::_drawWeatherRow() {
  _tft.fillRect(0, 20, 240, 38, CLR_CARD);

  if (!weather.valid) {
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);

    if (weather.errorMsg == "API key invalid") {
      _tft.setTextColor(CLR_RED, CLR_CARD);
      _tft.drawString("Weather: invalid API key", 120, 28);
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Fix key at /config", 120, 40);
    } else if (weather.errorMsg == "City not found") {
      _tft.setTextColor(CLR_RED, CLR_CARD);
      _tft.drawString("Weather: city not found", 120, 28);
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Fix city at /config", 120, 40);
    } else if (!weather.errorMsg.isEmpty()) {
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString(weather.errorMsg.c_str(), 120, 34);
    } else {
      _tft.setTextColor(CLR_SUBTEXT, CLR_CARD);
      _tft.drawString("Weather unavailable", 120, 34);
    }

    _tft.setTextDatum(TL_DATUM);
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
void DisplayManager::_drawCurrentTask() {
  _tft.fillRect(0, 58, 240, 210, CLR_BG);

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

  const float urgency = (t->durationMin > 0)
    ? 1.0f - ((float)sched.remainingSec() / (float)(t->durationMin * 60))
    : 1.0f;

  uint32_t arcColor;
  if (urgency > 0.8f) {
    arcColor = ((int)_gaugeAnimT % 2 == 0) ? CLR_RED : 0xA800;  // pulse
  } else if (urgency > 0.5f) {
    arcColor = CLR_YELLOW;
  } else {
    arcColor = CLR_ACCENT;
  }

  // ── Group label — size 1, centred, y=62 ──────────────────────────────────
  if (sched.currentGroup()) {
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
  const Task* next = sched.nextTask();
  _tft.fillRect(0, 240, 240, 28, CLR_CARD);
  _tft.drawFastHLine(0, 240, 240, CLR_SUBTEXT);
  if (next) {
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
                                float urgency, uint32_t arcColor) {
  const int STROKE = 10;
  const int r_arc  = r - STROKE / 2;

  // ── Track ring ────────────────────────────────────────────────────────────
  _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                     225, 495, CLR_GAUGE_BG, CLR_BG);

  // ── Remaining arc ─────────────────────────────────────────────────────────
  if (urgency < 1.0f) {
    float endDeg = 225.0f + 270.0f * (1.0f - urgency);
    _tft.drawSmoothArc(cx, cy, r_arc, r_arc - STROKE,
                       225, (int)endDeg, arcColor, CLR_BG);
  }

  // ── Emoji — slow bob ±2px ─────────────────────────────────────────────────
  int bobY = (int)(sinf(_gaugeAnimT * 1.6f) * 2.0f);

  // ASCII fallback faces (replace with NotoEmoji VLW for real emoji — see note)
  const char* face = (urgency > 0.8f) ? ":o"
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

// Legacy stub
void DisplayManager::_drawBunny(int a, int b, int d, float e, int f, int g) {
  (void)a;(void)b;(void)d;(void)e;(void)f;(void)g;
}

void DisplayManager::setScreen(Screen s) {
  _screen = s;
  _tft.fillScreen(CLR_BG);
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch polling
// Returns: -1=no touch, 0=main area, 1=All Tasks, 2=Mute,
//           3=Back (task list), 4=Weather row (clothing overlay)
// ─────────────────────────────────────────────────────────────────────────────
int DisplayManager::pollTouch() {
  uint16_t tx, ty;
  if (!_tft.getTouch(&tx, &ty)) return -1;

  if (_screen == Screen::TASK_LIST) {
    if (ty < 30) return 3;  // Back button
    return 0;
  }

  // HOME screen zones
  if (ty >= 20 && ty < 60) return 4;   // Weather row → clothing overlay
  if (ty > 262) {                       // Nav bar
    return (tx < 120) ? 1 : 2;
  }

  return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Clothing overlay — draws over the full screen, blocks until tap or timeout,
// then redraws the current screen.
// ─────────────────────────────────────────────────────────────────────────────
#include "ClothingAdvisor.h"

void DisplayManager::showClothingOverlay() {
  if (!weather.valid) return;  // No data — nothing to recommend

  auto items = ClothingAdvisor::recommend(weather);
  _drawClothingOverlay(items);

  // Block until tap or 8-second auto-dismiss
  uint32_t start = millis();
  while (millis() - start < 8000) {
    uint16_t tx, ty;
    if (_tft.getTouch(&tx, &ty)) {
      delay(200);  // debounce
      break;
    }
    delay(50);
  }

  // Redraw home screen
  time_t t = time(nullptr);
  struct tm* tm_now = localtime(&t);
  update(tm_now);
}

void DisplayManager::_drawClothingOverlay(const std::vector<ClothingItem>& items) {
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
  auto& cfg = Config::instance();
  char summary[40];
  snprintf(summary, sizeof(summary), "%.0f%s  %s",
           weather.temp,
           cfg.data.metricUnits ? "\xF7""C" : "\xF7""F",
           weather.description.substring(0, 16).c_str());
  _tft.setTextDatum(MC_DATUM);
  _tft.setTextColor(CLR_SUBTEXT, CLR_BG);
  _tft.setTextSize(1);
  _tft.drawString(summary, 120, 40);
  _tft.setTextDatum(TL_DATUM);

  // ── Item list ────────────────────────────────────────────────────────────
  int y        = 54;
  int maxItems = min((int)items.size(), 7);

  for (int i = 0; i < maxItems; i++) {
    uint32_t rowBg = (i % 2 == 0) ? CLR_CARD : CLR_BG;
    _tft.fillRect(0, y, 240, 30, rowBg);
    _tft.fillRect(0, y, 3, 30, CLR_ACCENT);  // Left accent bar

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
  if ((int)items.size() > maxItems) {
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
```

---

### ClothingAdvisor

Rule-based engine that derives clothing items from live weather data. Called by `DisplayManager::showClothingOverlay()` when the user taps the weather bar.

**`src/display/ClothingAdvisor.h`**
```cpp
#pragma once
#include <Arduino.h>
#include <vector>

// Forward declaration — avoid circular include with DisplayManager.h
struct WeatherData;

struct ClothingItem {
  String icon;   // Emoji rendered as text on TFT
  String label;
};

class ClothingAdvisor {
public:
  static std::vector<ClothingItem> recommend(const WeatherData& wx);

private:
  static bool _isRainy (const String& desc);
  static bool _isSnowy (const String& desc);
  static bool _isStormy(const String& desc);
};
```

**`src/display/ClothingAdvisor.cpp`**
```cpp
#include "ClothingAdvisor.h"
#include "DisplayManager.h"  // for WeatherData definition

bool ClothingAdvisor::_isRainy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("rain") >= 0 || d.indexOf("drizzle") >= 0 ||
         d.indexOf("shower") >= 0;
}

bool ClothingAdvisor::_isSnowy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("snow") >= 0 || d.indexOf("sleet") >= 0 ||
         d.indexOf("blizzard") >= 0;
}

bool ClothingAdvisor::_isStormy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("storm") >= 0 || d.indexOf("thunder") >= 0;
}

std::vector<ClothingItem> ClothingAdvisor::recommend(const WeatherData& wx) {
  std::vector<ClothingItem> items;
  float temp  = wx.temp;       // Metric °C; caller ensures correct units
  float wind  = wx.windSpeed;  // km/h
  bool  rainy = _isRainy(wx.description);
  bool  snowy = _isSnowy(wx.description);
  bool  stormy= _isStormy(wx.description);

  // ── Top layer ────────────────────────────────────────────────────────────
  if (snowy || temp < 0)   items.push_back({"🧥", "Heavy coat"});
  else if (temp < 8)       items.push_back({"🧥", "Winter coat"});
  else if (temp < 14)      items.push_back({"🧣", "Jacket + scarf"});
  else if (temp < 18)      items.push_back({"👕", "Light jacket"});
  else if (temp < 24)      items.push_back({"👕", "T-shirt / shirt"});
  else                     items.push_back({"👕", "Light top"});

  // ── Bottom layer ─────────────────────────────────────────────────────────
  if (temp < 8 || snowy)   items.push_back({"👖", "Thermal trousers"});
  else if (temp < 18)      items.push_back({"👖", "Trousers / jeans"});
  else                     items.push_back({"🩳", "Shorts / light trousers"});

  // ── Footwear ─────────────────────────────────────────────────────────────
  if (snowy || temp < 2)       items.push_back({"🥾", "Winter boots"});
  else if (rainy || stormy)    items.push_back({"🥾", "Waterproof shoes"});
  else if (temp > 22)          items.push_back({"👟", "Trainers / sandals"});
  else                         items.push_back({"👟", "Trainers"});

  // ── Rain gear ────────────────────────────────────────────────────────────
  if (stormy)                  items.push_back({"⛈",  "Stay indoors if possible"});
  else if (rainy)              items.push_back({"☂️", "Umbrella"});

  // ── Wind chill ───────────────────────────────────────────────────────────
  if (wind > 30 && temp < 15)  items.push_back({"🧣", "Scarf / windbreaker"});
  if (wind > 20 && temp < 10)  items.push_back({"🧤", "Gloves"});

  // ── Sun ──────────────────────────────────────────────────────────────────
  if (temp > 24)               items.push_back({"🕶️", "Sunglasses"});
  if (temp > 28)               items.push_back({"🧴", "Sunscreen"});

  // ── Humidity ─────────────────────────────────────────────────────────────
  if (wx.humidity > 80 && temp > 22)
                               items.push_back({"💧", "Stay hydrated"});

  return items;
}
```

**Recommendation logic summary:**

| Condition | Items added |
|-----------|-------------|
| temp < 0 / snow | Heavy coat, thermal trousers, winter boots |
| temp 0–8 | Winter coat, trousers/jeans, trainers |
| temp 8–14 | Jacket + scarf |
| temp 14–18 | Light jacket |
| temp 18–24 | T-shirt, trousers or shorts |
| temp > 24 | Light top, shorts, sunglasses |
| temp > 28 | + Sunscreen |
| rain / drizzle | Waterproof shoes, umbrella |
| storm / thunder | "Stay indoors if possible" |
| wind > 30 km/h + cold | Scarf / windbreaker |
| wind > 20 km/h + very cold | + Gloves |
| humidity > 80% + hot | "Stay hydrated" |

---

### BacklightManager

Controls the ILI9341 backlight via PWM. Auto-dims after a configurable idle timeout and wakes instantly on touch. `BacklightManager::wake()` is called from the touch handler in `main.cpp` — no extra logic needed at the call site.

**Hardware:** Connect the TFT's `BL` (backlight) pin to **GPIO 7** (or any PWM-capable GPIO not already used). The ILI9341 backlight accepts 3.3V PWM directly — no transistor needed at these current levels. If brightness feels weak, add a small NPN transistor (2N2222 or similar) driven from the GPIO.

**`src/display/BacklightManager.h`**
```cpp
#pragma once
#include <Arduino.h>

class BacklightManager {
public:
  static BacklightManager& instance();

  void begin(int pin, uint8_t fullBrightness = 255,
             uint32_t dimAfterMs = 60000,
             uint8_t  dimBrightness = 30);

  void wake();          // Call on any touch event — resets idle timer, full brightness
  void tick();          // Call every loop() — handles fade timing

  void setBrightness(uint8_t val);   // 0=off, 255=full
  bool isDimmed() const { return _dimmed; }

private:
  BacklightManager() {}

  int      _pin            = -1;
  uint8_t  _fullBrightness = 255;
  uint8_t  _dimBrightness  = 30;
  uint32_t _dimAfterMs     = 60000;
  uint32_t _lastActivity   = 0;
  bool     _dimmed         = false;
  uint8_t  _current        = 255;

  static constexpr int  PWM_CHANNEL = 0;   // LEDC channel — don't clash with others
  static constexpr int  PWM_FREQ    = 5000; // Hz — above audible range
  static constexpr int  PWM_RES     = 8;    // bits — 0–255
};
```

**`src/display/BacklightManager.cpp`**
```cpp
#include "BacklightManager.h"

BacklightManager& BacklightManager::instance() {
  static BacklightManager inst;
  return inst;
}

void BacklightManager::begin(int pin, uint8_t fullBrightness,
                              uint32_t dimAfterMs, uint8_t dimBrightness) {
  _pin            = pin;
  _fullBrightness = fullBrightness;
  _dimAfterMs     = dimAfterMs;
  _dimBrightness  = dimBrightness;
  _lastActivity   = millis();

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(_pin, PWM_CHANNEL);
  setBrightness(_fullBrightness);

  Serial.printf("[Backlight] Init on GPIO %d, dim after %lums\n", _pin, _dimAfterMs);
}

void BacklightManager::setBrightness(uint8_t val) {
  _current = val;
  ledcWrite(PWM_CHANNEL, val);
}

void BacklightManager::wake() {
  _lastActivity = millis();
  if (_dimmed) {
    _dimmed = false;
    setBrightness(_fullBrightness);
    Serial.println("[Backlight] Wake");
  }
}

void BacklightManager::tick() {
  if (_dimmed) return;  // Already dimmed — nothing to do until wake()

  uint32_t idle = millis() - _lastActivity;

  if (idle >= _dimAfterMs) {
    // Smooth fade — step down 5 units every tick until dim level reached
    if (_current > _dimBrightness) {
      uint8_t next = (_current > _dimBrightness + 5)
                     ? _current - 5
                     : _dimBrightness;
      setBrightness(next);
    } else {
      _dimmed = true;
      Serial.println("[Backlight] Dimmed");
    }
  }
}
```

> **Wiring:** `TFT BL pin → GPIO 7`. The ILI9341's backlight LED draws ~60mA at full brightness — within the ESP32-C3 GPIO source limit (40mA per pin). If you want full 255-level brightness without a current drop, wire through a 2N2222 NPN transistor: ESP32 GPIO → 1kΩ → transistor base; collector → TFT BL; emitter → GND.
>
> **Dim timeout** is configurable — default 60 seconds. Set a shorter value (e.g. 30s) in the web config for better battery life overnight.

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
    auto& wx  = DisplayManager::instance().weather;
    JsonDocument doc;
    doc["timezone"] = cfg.data.timezone;
    doc["city"]     = cfg.data.city;
    doc["metric"]   = cfg.data.metricUnits;
    doc["muted"]    = cfg.data.muted;

    // Expose whether a key exists and its last 6 chars so the UI can show
    // a masked hint (e.g. "…a3f92c") without putting the full key in the page.
    // The full key is never sent over HTTP.
    if (!cfg.data.owmApiKey.isEmpty()) {
      doc["owmkey_set"]  = true;
      doc["owmkey_hint"] = "…" + cfg.data.owmApiKey.substring(
                             cfg.data.owmApiKey.length() - 6);
    } else {
      doc["owmkey_set"]  = false;
      doc["owmkey_hint"] = "";
    }

    // Expose current weather error so the UI can highlight the key field
    // when the key has been revoked or the city name is wrong.
    doc["weather_error"] = wx.errorMsg;

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
      if (doc["city"].is<String>()) {
        String newCity = doc["city"].as<String>();
        newCity.trim();
        if (!newCity.isEmpty()) cfg.data.city = newCity;
      }
      if (doc["metric"].is<bool>()) cfg.data.metricUnits = doc["metric"].as<bool>();

      // Only overwrite the OWM key if a non-empty value was submitted.
      // An absent or empty "owmkey" field means "keep the existing key" —
      // this prevents a blank config save from accidentally wiping a working key.
      bool keyChanged = false;
      if (doc["owmkey"].is<String>()) {
        String newKey = doc["owmkey"].as<String>();
        newKey.trim();
        if (!newKey.isEmpty()) {
          cfg.data.owmApiKey = newKey;
          keyChanged = true;
        }
      }

      cfg.save();

      // If city or key changed, reset weather backoff so a fetch is
      // attempted immediately on the next loop iteration.
      if (keyChanged || doc["city"].is<String>()) {
        extern WxState  wxState;
        extern uint32_t wxBackoffUntil;
        extern uint32_t lastWxFetch;
        wxState        = WxState::OK;
        wxBackoffUntil = 0;
        lastWxFetch    = 0;

        // Clear any existing error message so the display stops showing it
        DisplayManager::instance().weather.errorMsg = "";
      }

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
      if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
      }

      // ── SSID ──────────────────────────────────────────────────────────────
      String ssid = doc["ssid"] | "";
      ssid.trim();
      if (ssid.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"ssid_required\"}");
        return;
      }
      if (ssid.length() > 32) {
        req->send(400, "application/json", "{\"error\":\"ssid_too_long\"}");
        return;
      }

      // ── Password ──────────────────────────────────────────────────────────
      String password = doc["password"] | "";
      if (password.length() > 0 && password.length() < 8) {
        req->send(400, "application/json", "{\"error\":\"password_too_short\"}");
        return;
      }
      if (password.length() > 63) {
        req->send(400, "application/json", "{\"error\":\"password_too_long\"}");
        return;
      }

      // ── Timezone ──────────────────────────────────────────────────────────
      String tz = doc["timezone"] | "UTC0";
      if (tz.isEmpty() || tz.length() > 64) tz = "UTC0";

      // ── City ──────────────────────────────────────────────────────────────
      String city = doc["city"] | "London";
      city.trim();
      if (city.isEmpty()) city = "London";
      if (city.length() > 85) city = city.substring(0, 85);

      // ── OWM key (optional) ────────────────────────────────────────────────
      // Accept empty (user skips weather for now) or exactly 32 hex chars.
      // If malformed but non-empty, save it with a warning — user can fix
      // later in /config without having to redo the whole WiFi setup.
      String owmkey = doc["owmkey"] | "";
      owmkey.trim();
      bool owmSuspect = (!owmkey.isEmpty() && owmkey.length() != 32);

      // ── All valid — save and reboot ───────────────────────────────────────
      auto& cfg = Config::instance();
      cfg.data.wifiSSID     = ssid;
      cfg.data.wifiPassword = password;
      cfg.data.timezone     = tz;
      cfg.data.city         = city;
      cfg.data.owmApiKey    = owmkey;
      cfg.data.metricUnits  = doc["metric"] | true;
      cfg.save();

      String resp = owmSuspect
        ? "{\"ok\":true,\"warn\":\"owmkey_suspect\",\"msg\":\"Saved. Rebooting...\"}"
        : "{\"ok\":true,\"msg\":\"Saved. Rebooting...\"}";

      req->send(200, "application/json", resp);
      delay(800);
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
  // Always emit weather_error — empty string means no error.
  // Web dashboard uses this to show a warning banner with a /config link.
  doc["weather_error"] = wx.errorMsg;

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

  // Touch feedback — generates a short tone directly into the I2S DMA buffer.
  // Non-blocking: fills one DMA buffer worth of samples and returns immediately.
  // freq: tone frequency in Hz (default 1200Hz — crisp without being harsh)
  // durationMs: tone length in milliseconds (default 18ms)
  void beep(uint16_t freq = 1200, uint16_t durationMs = 18);

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

// ── Touch beep ────────────────────────────────────────────────────────────────
// Synthesises a pure square wave directly into a stack buffer and writes it
// to the I2S DMA in one call. No file I/O, no heap allocation.
// Does nothing if audio is muted (checked by the caller in main.cpp).
void AudioManager::beep(uint16_t freq, uint16_t durationMs) {
  // Reconfigure I2S clock for 16kHz — higher sample rate = cleaner beep
  i2s_set_clk(I2S_PORT, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  const uint32_t sampleRate  = 16000;
  const uint32_t totalSamples= (sampleRate * durationMs) / 1000;
  const uint32_t halfPeriod  = sampleRate / (2 * freq);  // samples per half-cycle

  // Stack buffer — 16kHz * 18ms = 288 samples * 2 bytes = 576 bytes
  // Safe on the stack; keep durationMs ≤ 50 to avoid overflow
  const size_t bufSize = (totalSamples * sizeof(int16_t));
  int16_t buf[totalSamples];

  int16_t amplitude = 8000;  // ~25% of int16 max — loud enough, not distorted
  for (uint32_t i = 0; i < totalSamples; i++) {
    buf[i] = ((i / halfPeriod) % 2 == 0) ? amplitude : -amplitude;
  }

  size_t written = 0;
  i2s_write(I2S_PORT, buf, bufSize, &written, pdMS_TO_TICKS(50));

  // Restore to default 8kHz for WAV playback
  i2s_set_clk(I2S_PORT, 8000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  i2s_zero_dma_buffer(I2S_PORT);
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
#include "display/BacklightManager.h"
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

// ─── Weather error state ──────────────────────────────────────────────────────
enum class WxState { OK, BAD_KEY, BAD_CITY, RATE_LIMITED, SERVER_ERROR };
WxState  wxState        = WxState::OK;
uint32_t wxBackoffUntil = 0;

// ─── Weather cache ────────────────────────────────────────────────────────────
// Persists the last successful fetch to LittleFS so the display shows real data
// immediately on boot, even before WiFi connects or the first fetch completes.
// Format: JSON with all WeatherData fields + a Unix timestamp.
static constexpr const char* WX_CACHE_PATH = "/weather_cache.json";

void saveWeatherCache() {
  auto& wx = DisplayManager::instance().weather;
  if (!wx.valid) return;

  JsonDocument doc;
  doc["temp"]        = wx.temp;
  doc["feelsLike"]   = wx.feelsLike;
  doc["humidity"]    = wx.humidity;
  doc["windSpeed"]   = wx.windSpeed;
  doc["description"] = wx.description;
  doc["icon"]        = wx.icon;
  doc["cachedAt"]    = (uint32_t)time(nullptr);  // Unix timestamp

  File f = LittleFS.open(WX_CACHE_PATH, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
    Serial.println("[Weather] Cache saved");
  }
}

void loadWeatherCache() {
  File f = LittleFS.open(WX_CACHE_PATH, "r");
  if (!f) {
    Serial.println("[Weather] No cache file");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, f)) { f.close(); return; }
  f.close();

  auto& wx = DisplayManager::instance().weather;
  wx.temp        = doc["temp"]        | 0.0f;
  wx.feelsLike   = doc["feelsLike"]   | 0.0f;
  wx.humidity    = doc["humidity"]    | 0;
  wx.windSpeed   = doc["windSpeed"]   | 0.0f;
  wx.description = doc["description"] | String("Cached");
  wx.icon        = doc["icon"]        | String("01d");
  wx.valid       = true;
  wx.errorMsg    = "";

  // Calculate how stale the cache is
  uint32_t cachedAt  = doc["cachedAt"] | 0;
  uint32_t nowUnix   = (uint32_t)time(nullptr);
  uint32_t ageMin    = (nowUnix > cachedAt) ? (nowUnix - cachedAt) / 60 : 0;

  Serial.printf("[Weather] Cache loaded — %u min old: %.1f° %s\n",
                ageMin, wx.temp, wx.description.c_str());

  // If cache is older than 4 hours, mark valid but flag it as stale so
  // the display shows a subtle "last updated Xh ago" note
  if (ageMin > 240) {
    wx.errorMsg = "Cached " + String(ageMin / 60) + "h ago";
  }
}

// ─── Weather ─────────────────────────────────────────────────────────────────
void fetchWeather() {
  auto& cfg = Config::instance();
  auto& wx  = DisplayManager::instance().weather;

  // Skip if we're in a backoff window
  if (millis() < wxBackoffUntil) return;

  // Skip if key or city is missing — not an error, just not configured yet
  if (cfg.data.owmApiKey.isEmpty() || cfg.data.city.isEmpty()) {
    wx.valid    = false;
    wx.errorMsg = "";
    return;
  }

  HTTPClient http;
  String units = cfg.data.metricUnits ? "metric" : "imperial";
  String url   = "http://api.openweathermap.org/data/2.5/weather?q=" +
                 cfg.data.city + "&units=" + units +
                 "&appid=" + cfg.data.owmApiKey;

  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();

  Serial.printf("[Weather] HTTP %d\n", code);

  switch (code) {
    case 200: {
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
        wx.errorMsg    = "";
        wxState        = WxState::OK;
        wxBackoffUntil = 0;
        Serial.printf("[Weather] OK: %.1f° %s\n", wx.temp, wx.description.c_str());
        saveWeatherCache();  // Persist to flash for next boot
      }
      break;
    }

    case 401:
      // Key invalid or revoked — stop retrying until user fixes key in /config
      wx.valid       = false;
      wx.errorMsg    = "API key invalid";
      wxState        = WxState::BAD_KEY;
      wxBackoffUntil = UINT32_MAX;  // Never auto-retry
      Serial.println("[Weather] 401 — API key invalid or revoked. Fix in /config");
      break;

    case 404:
      // City not found — stop retrying until user fixes city in /config
      wx.valid       = false;
      wx.errorMsg    = "City not found";
      wxState        = WxState::BAD_CITY;
      wxBackoffUntil = UINT32_MAX;  // Never auto-retry
      Serial.println("[Weather] 404 — City not found. Fix in /config");
      break;

    case 429:
      // Free tier rate limit — back off 60 minutes (shouldn't happen at 20-min intervals)
      wx.errorMsg    = "Rate limited";
      wxState        = WxState::RATE_LIMITED;
      wxBackoffUntil = millis() + 60UL * 60 * 1000;
      Serial.println("[Weather] 429 — Rate limited. Backing off 60 min");
      break;

    default:
      if (code >= 500) {
        // OWM server error — back off 10 minutes
        wx.errorMsg    = "Weather service error";
        wxState        = WxState::SERVER_ERROR;
        wxBackoffUntil = millis() + 10UL * 60 * 1000;
        Serial.printf("[Weather] %d — Server error. Backing off 10 min\n", code);
      } else if (code < 0) {
        // Connection failed (WiFi down) — back off 2 minutes, don't clear valid data
        wx.errorMsg    = "No connection";
        wxBackoffUntil = millis() + 2UL * 60 * 1000;
        Serial.println("[Weather] Connection failed. Backing off 2 min");
      }
      break;
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
  BacklightManager::instance().begin(/*pin=*/7, /*fullBrightness=*/255,
                                     /*dimAfterMs=*/60000, /*dimBrightness=*/30);

  // Load weather cache before WiFi connects so display shows data immediately
  loadWeatherCache();

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
  // fetchWeather() internally checks wxBackoffUntil — for permanent errors
  // (bad key, bad city) it will no-op until the user fixes config and the
  // POST /api/config handler resets wxBackoffUntil to 0.
  if (!apMode && now_ms - lastWxFetch >= 20UL * 60 * 1000) {
    lastWxFetch = now_ms;
    fetchWeather();
  }

  // ── Backlight auto-dim ─────────────────────────────────────────────────────
  BacklightManager::instance().tick();

  // ── Touch input ────────────────────────────────────────────────────────────
  static uint32_t lastTouch = 0;
  if (now_ms - lastTouch > 300) {   // 300ms debounce
    int zone = DisplayManager::instance().pollTouch();
    if (zone >= 0) {
      lastTouch = now_ms;

      // Wake backlight on any touch — do this first so feedback feels instant
      BacklightManager::instance().wake();

      // Touch beep (skip if muted)
      if (!Config::instance().data.muted) {
        AudioManager::instance().beep(1200, 18);
      }

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
        case 4:  // Weather row — show clothing overlay
          DisplayManager::instance().showClothingOverlay();
          break;
      }
    }
  }

  // ── Audio ──────────────────────────────────────────────────────────────────
  AudioManager::instance().loop();
}
```

---

## Screen Mockups

A companion file `screens.html` renders all device and web portal screens as interactive HTML mockups at exact pixel dimensions, with tab navigation between states. Open it directly in a browser — no server needed.

```
screens.html    ← open in any browser
```

### Screens Included

| Tab | What it shows |
|-----|---------------|
| **Home — Active Task** | Status bar, weather row, group label, task name, 270° dial arc with 😊 emoji bobbing inside, time remaining + "of X min", next task strip |
| **Home — Sprint Mode** | <20% remaining — arc pulses red, emoji switches to 😱 |
| **Auto-Dimmed** | BacklightManager has faded to brightness 30/255 after 60s idle — ghost clock at low opacity, "tap to wake" hint |
| **Home — Idle** | No active task; shows next upcoming task name and time until it starts |
| **Home — Muted** | Red MUTED badge, nav mute turns red, battery yellow, dial arc yellow with 😰 |
| **Home — Weather Error** | Weather row turns red with "invalid API key / Fix at /config" — task and bunny continue normally |
| **Home — Stale Cache** | Weather loaded from cache (>4h old) — row dims with amber "📦 Cached 5h ago" note, timeline continues normally |
| **Clothing Overlay** | Full-screen "What to Wear" overlay triggered by tapping the weather row |
| **All Tasks** | Scrollable grouped list — green dot = completed, cyan highlight = active, collapsed groups |
| **Captive Portal (device)** | What the physical display shows on first boot in AP mode |
| **Web Dashboard** | Browser live dashboard with WebSocket status indicator and weather error banner element |
| **Web Config** | Schedule editor showing OWM key masked hint (`…a3f92c · active`) with Replace button |
| **Web Setup Portal** | First-run form at `192.168.4.1` for WiFi + timezone + weather config |

### Colour Palette

All mockup colours are derived directly from the firmware's RGB565 constants in `DisplayManager.h`:

| Variable | Hex | Usage |
|----------|-----|-------|
| `CLR_BG` | `#101828` | Screen background |
| `CLR_CARD` | `#1a2235` | Card / weather row background |
| `CLR_STATUSBG` | `#0d1420` | Status bar background |
| `CLR_ACCENT` | `#00c8ff` | Cyan — progress bar, active task, WiFi arcs |
| `CLR_TEXT` | `#e8f0fe` | Primary text |
| `CLR_SUBTEXT` | `#6b7fa3` | Secondary / dim text |
| `CLR_GREEN` | `#34d97b` | Battery full, completed task dot |
| `CLR_YELLOW` | `#f5c542` | Battery mid (20–50%) |
| `CLR_RED` | `#f25c5c` | Battery low, mute active, weather error |
| `CLR_BAR_BG` | `#243047` | Progress bar track |

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

  .weather-card { cursor: pointer; }
  .weather-card:hover { border-color: var(--accent); }

  .clothing-panel {
    background: var(--card); border-radius: 12px;
    overflow: hidden; margin-bottom: 12px;
  }
  .clothing-panel-header {
    display: flex; justify-content: space-between; align-items: center;
    padding: 10px 14px;
    font-size: 13px; font-weight: 600; color: var(--text);
    border-bottom: 1px solid var(--bar-bg);
  }
  .clothing-close {
    background: none; border: none; color: var(--sub);
    font-size: 14px; cursor: pointer; padding: 0 4px;
  }
  .clothing-item {
    display: flex; align-items: center; gap: 12px;
    padding: 9px 14px;
    border-bottom: 1px solid var(--bar-bg);
    font-size: 14px; color: var(--text);
  }
  .clothing-item:last-child { border-bottom: none; }
  .clothing-item-icon { font-size: 20px; width: 28px; text-align: center; flex-shrink: 0; }
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

<div class="weather-card" id="weatherCard" style="display:none" onclick="toggleClothing()" title="Tap for clothing recommendations">
  <div>
    <div class="weather-temp" id="wxTemp">--°</div>
    <div style="font-size:13px;color:var(--sub)" id="wxDesc">--</div>
  </div>
  <div class="weather-details">
    <div id="wxHumid">💧 --%</div>
    <div id="wxWind">💨 -- km/h</div>
  </div>
  <div style="font-size:11px;color:var(--sub);margin-left:8px;opacity:0.6">👗</div>
</div>

<!-- Clothing recommendations panel — shown/hidden on weather card tap -->
<div class="clothing-panel" id="clothingPanel-wrap" style="display:none">
  <div class="clothing-panel-header">
    <span>👗 What to Wear</span>
    <button class="clothing-close" onclick="toggleClothing()">✕</button>
  </div>
  <div id="clothingPanel"></div>
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
    _lastWeatherData = d.weather;
    document.getElementById('weatherCard').style.display = 'flex';
    document.getElementById('wxTemp').textContent  = `${Math.round(d.weather.temp)}°`;
    document.getElementById('wxDesc').textContent  = d.weather.desc || '';
    document.getElementById('wxHumid').textContent = `💧 ${d.weather.humid}%`;
    document.getElementById('wxWind').textContent  = `💨 ${Math.round(d.weather.wind)} km/h`;
    // Keep clothing panel up-to-date if it's open
    if (document.getElementById('clothingPanel-wrap').style.display !== 'none') {
      updateClothing(d.weather);
    }
  }

  // Weather error banner — shown when key is revoked, city is wrong, etc.
  const wxErr = document.getElementById('wxErrorBanner');
  if (d.weather_error) {
    wxErr.style.display = 'block';
    wxErr.innerHTML = `⚠ Weather: ${d.weather_error}. <a href="/config.html">Fix in Config →</a>`;
  } else {
    wxErr.style.display = 'none';
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

// ── Clothing recommendations ─────────────────────────────────────────────────
let _lastWeatherData = null;

function toggleClothing() {
  const wrap = document.getElementById('clothingPanel-wrap');
  const isVisible = wrap.style.display !== 'none';
  wrap.style.display = isVisible ? 'none' : 'block';
  if (!isVisible && _lastWeatherData) updateClothing(_lastWeatherData);
}

function updateClothing(wx) {
  const items = recommendClothing(wx);
  document.getElementById('clothingPanel').innerHTML = items
    .map(i => `<div class="clothing-item">
                 <span class="clothing-item-icon">${i.icon}</span>
                 <span>${i.label}</span>
               </div>`)
    .join('');
}

function recommendClothing(wx) {
  const temp   = wx.temp;
  const wind   = wx.wind  || 0;
  const humid  = wx.humid || 0;
  const desc   = (wx.desc || '').toLowerCase();
  const rainy  = /rain|drizzle|shower/.test(desc);
  const snowy  = /snow|sleet|blizzard/.test(desc);
  const stormy = /storm|thunder/.test(desc);
  const items  = [];

  // Top layer
  if (temp < 0 || snowy)    items.push({icon:'🧥', label:'Heavy coat'});
  else if (temp < 8)        items.push({icon:'🧥', label:'Winter coat'});
  else if (temp < 14)       items.push({icon:'🧣', label:'Jacket + scarf'});
  else if (temp < 18)       items.push({icon:'👕', label:'Light jacket'});
  else if (temp < 24)       items.push({icon:'👕', label:'T-shirt / shirt'});
  else                      items.push({icon:'👕', label:'Light top'});

  // Bottom layer
  if (temp < 8 || snowy)    items.push({icon:'👖', label:'Thermal trousers'});
  else if (temp < 18)       items.push({icon:'👖', label:'Trousers / jeans'});
  else                      items.push({icon:'🩳', label:'Shorts / light trousers'});

  // Footwear
  if (snowy || temp < 2)    items.push({icon:'🥾', label:'Winter boots'});
  else if (rainy || stormy) items.push({icon:'🥾', label:'Waterproof shoes'});
  else if (temp > 22)       items.push({icon:'👟', label:'Trainers / sandals'});
  else                      items.push({icon:'👟', label:'Trainers'});

  // Rain / storm
  if (stormy)               items.push({icon:'⛈',  label:'Stay indoors if possible'});
  else if (rainy)           items.push({icon:'☂️', label:'Umbrella'});

  // Wind chill
  if (wind > 30 && temp < 15) items.push({icon:'🧣', label:'Scarf / windbreaker'});
  if (wind > 20 && temp < 10) items.push({icon:'🧤', label:'Gloves'});

  // Sun
  if (temp > 24)            items.push({icon:'🕶️', label:'Sunglasses'});
  if (temp > 28)            items.push({icon:'🧴', label:'Sunscreen'});

  // Hydration
  if (humid > 80 && temp > 22) items.push({icon:'💧', label:'Stay hydrated'});

  return items;
}

connect();
</script>
</body>
</html>
```

---

### Setup Portal Validation (`data/setup.html`)

Client-side validation runs before the POST fires. The firmware also validates server-side independently.

**Validation rules:**

| Field | Rule |
|-------|------|
| SSID | Required, 1–32 chars, no leading/trailing spaces |
| Password | Empty = open network (allowed); if provided must be 8–63 chars (WPA2 spec) |
| Timezone | Must be selected from dropdown — not free text |
| City | Required, 2–85 chars, letters/spaces/hyphens only |
| OWM API key | Optional; if provided must be exactly 32 lowercase hex characters |

**`data/setup.html` — validation script**
```javascript
function validate() {
  const ssid     = document.getElementById('ssid').value.trim();
  const password = document.getElementById('password').value;
  const city     = document.getElementById('city').value.trim();
  const owmkey   = document.getElementById('owmkey').value.trim();
  const errors   = [];

  if (!ssid)
    errors.push('WiFi network name is required.');
  else if (ssid.length > 32)
    errors.push('WiFi network name must be 32 characters or fewer.');
  else if (ssid !== ssid.trim())
    errors.push('WiFi network name cannot start or end with spaces.');

  if (password.length > 0 && password.length < 8)
    errors.push('WiFi password must be at least 8 characters (WPA2 minimum).');
  if (password.length > 63)
    errors.push('WiFi password must be 63 characters or fewer.');

  if (!city)
    errors.push('City is required for weather.');
  else if (!/^[a-zA-Z\s\-'.]{2,85}$/.test(city))
    errors.push('City name contains invalid characters.');

  if (owmkey && !/^[a-f0-9]{32}$/.test(owmkey))
    errors.push('OpenWeatherMap API key should be 32 hex characters — check you copied it fully.');

  if (errors.length > 0) {
    document.getElementById('error-box').innerHTML =
      errors.map(e => `<div class="error-item">⚠ ${e}</div>`).join('');
    document.getElementById('error-box').style.display = 'block';
    return false;
  }
  document.getElementById('error-box').style.display = 'none';
  return true;
}

async function submit() {
  if (!validate()) return;

  const btn = document.getElementById('submit-btn');
  btn.textContent = 'Saving…';
  btn.disabled = true;

  const payload = {
    ssid:     document.getElementById('ssid').value.trim(),
    password: document.getElementById('password').value,
    timezone: document.getElementById('timezone').value,
    city:     document.getElementById('city').value.trim(),
    owmkey:   document.getElementById('owmkey').value.trim(),
    metric:   document.getElementById('units').value === 'metric',
  };

  try {
    const res  = await fetch('/save-setup', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    const data = await res.json();

    if (!res.ok) {
      document.getElementById('error-box').innerHTML =
        `<div class="error-item">⚠ ${data.error || 'Unknown error from device.'}</div>`;
      document.getElementById('error-box').style.display = 'block';
      btn.textContent = 'Save & Connect →';
      btn.disabled = false;
    } else {
      if (data.warn === 'owmkey_suspect') {
        btn.textContent = '✓ Saved (check API key) — rebooting…';
      } else {
        btn.textContent = '✓ Saved — rebooting device…';
      }
      // Device disconnects as it reboots — page goes offline naturally
    }
  } catch (e) {
    // Connection dropped because device rebooted — that's expected
    btn.textContent = '✓ Saved — device is connecting…';
  }
}
```

**Server-side error codes returned by `POST /save-setup`:**

| Code | Meaning |
|------|---------|
| `ssid_required` | SSID field was empty |
| `ssid_too_long` | SSID exceeded 32 characters |
| `password_too_short` | Password provided but under 8 characters |
| `password_too_long` | Password exceeded 63 characters |
| `invalid_json` | POST body could not be parsed |
| `warn: owmkey_suspect` | Key saved but length was not 32 chars — soft warning, not a failure |

---

### OWM Key Management (`data/config.html`)

The config page shows a masked hint of the active key (last 6 chars) rather than a blank field. The full key is never sent to the browser. Replacing the key requires an explicit **Replace** button tap — saving the form without clicking Replace leaves the existing key untouched.

**Key field states:**

```
No key set
  └─► Empty input with placeholder "Paste 32-character API key"

Key is set (normal)
  └─► "…a3f92c  ● active   [Replace]"

Key is set but weather_error = "API key invalid"
  └─► "…a3f92c  ● invalid — replace key   [Replace]"   ← red dot + red label

Replacing (after clicking Replace)
  └─► Text input, live 0/32 character counter, [Cancel] link
       ├─ 0 chars:  counter neutral
       ├─ 1–31 chars: counter red
       └─ 32 chars:  counter green
```

**`data/config.html` — OWM key field script**
```javascript
let owmKeyIsSet     = false;
let owmKeyReplacing = false;

// Call this after GET /api/config returns
function initOwmField(config) {
  owmKeyIsSet = config.owmkey_set;

  if (owmKeyIsSet) {
    document.getElementById('owm-hint-text').textContent = config.owmkey_hint;

    if (config.weather_error === 'API key invalid') {
      const s = document.getElementById('owm-status-text');
      s.textContent = 'invalid — replace key';
      s.style.color = 'var(--red)';
      document.querySelector('.owm-dot').style.background = 'var(--red)';
    }

    document.getElementById('owm-hint-row').style.display  = 'flex';
    document.getElementById('owm-input-row').style.display = 'none';
  } else {
    document.getElementById('owm-hint-row').style.display  = 'none';
    document.getElementById('owm-input-row').style.display = 'block';
  }
}

function startReplaceKey() {
  owmKeyReplacing = true;
  document.getElementById('owm-hint-row').style.display  = 'none';
  document.getElementById('owm-input-row').style.display = 'block';
  document.getElementById('owm-cancel-link').style.display = 'inline';
  document.getElementById('owm-input').focus();
}

function cancelReplaceKey() {
  owmKeyReplacing = false;
  document.getElementById('owm-input').value = '';
  document.getElementById('owm-input-row').style.display = 'none';
  document.getElementById('owm-hint-row').style.display  = 'flex';
  document.getElementById('owm-cancel-link').style.display = 'none';
  document.getElementById('owm-error').style.display = 'none';
}

// Live character counter
document.getElementById('owm-input').addEventListener('input', function() {
  const n = this.value.trim().length;
  const counter = document.getElementById('owm-char-count');
  counter.textContent = `${n} / 32`;
  counter.style.color = n === 0 ? '' : n === 32 ? 'var(--green)' : 'var(--red)';
});

// Returns: string = new key to send | null = no change | undefined = validation error
function getOwmKeyForSave() {
  const input = document.getElementById('owm-input').value.trim();

  if (!owmKeyIsSet) {
    // No key set — empty is fine (skips weather), any non-empty value gets validated
    if (input && !/^[a-f0-9]{32}$/.test(input)) {
      document.getElementById('owm-error').textContent =
        'Key must be 32 lowercase hex characters.';
      document.getElementById('owm-error').style.display = 'block';
      return undefined;
    }
    return input || null;
  }

  if (owmKeyReplacing) {
    if (!input) {
      document.getElementById('owm-error').textContent =
        'Enter a new key, or tap Cancel to keep the existing one.';
      document.getElementById('owm-error').style.display = 'block';
      return undefined;
    }
    if (!/^[a-f0-9]{32}$/.test(input)) {
      document.getElementById('owm-error').textContent =
        'Key must be 32 lowercase hex characters. Check you copied it fully.';
      document.getElementById('owm-error').style.display = 'block';
      return undefined;
    }
    document.getElementById('owm-error').style.display = 'none';
    return input;
  }

  return null;  // Key set, user didn't click Replace — leave untouched
}

async function saveConfig() {
  const owmKey = getOwmKeyForSave();
  if (owmKey === undefined) return;  // Validation failed

  const payload = {
    timezone: document.getElementById('timezone').value,
    city:     document.getElementById('city').value.trim(),
    metric:   document.getElementById('units').value === 'metric',
  };
  if (owmKey !== null) payload.owmkey = owmKey;

  const res = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });

  if (res.ok) {
    // Reload to refresh masked hint with new key tail
    await loadConfig();
    showToast('Settings saved');
  }
}

async function loadConfig() {
  const res    = await fetch('/api/config');
  const config = await res.json();
  document.getElementById('timezone').value = config.timezone || 'UTC0';
  document.getElementById('city').value     = config.city     || '';
  document.getElementById('units').value    = config.metric   ? 'metric' : 'imperial';
  initOwmField(config);
}

loadConfig();
```

**POST payload behaviour:**

| Scenario | Payload includes `owmkey`? | Result |
|----------|---------------------------|--------|
| Key not set, user pastes new key | Yes | Key saved, backoff reset, fetch triggered |
| Key not set, user leaves blank | No | No key saved, weather stays unconfigured |
| Key set, user never clicks Replace | No | Existing key untouched |
| Key set, user clicks Replace + pastes | Yes | Key overwritten, backoff reset, fetch triggered |
| Key set, user clicks Replace + cancels | No | Existing key untouched |

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
- **Bunny emoji rendering:** TFT_eSPI renders the bunny as primitive shapes — smooth anti-aliasing is not available without a custom font or sprite sheet. The animation is readable but not pixel-art quality. For higher fidelity, store a 4-frame 28×22px RLE sprite in flash.
- **Beep during WAV playback:** `beep()` reconfigures the I2S clock, which will interrupt any in-progress WAV playback. In practice this is fine since touch events don't fire during a voice prompt, but be aware if you add background audio.
- **Backlight transistor:** At full brightness the ILI9341 backlight draws ~60mA, which is at the ESP32-C3 GPIO source limit. For sustained full brightness, add a 2N2222 NPN transistor as noted in BacklightManager.
- **Weather cache age display:** The `errorMsg = "Cached Xh ago"` stale flag shows in the weather row on the display. It does not block the clothing overlay — recommendations still use the cached data.
- **Dynamic audio:** Task names use generic prompts; for name-specific voice ("Starting: Drink Coffee"), implement clip concatenation or add a small TTS cache
- **OTA updates:** Add ArduinoOTA for wireless firmware updates — saves repeated USB connections once the case is assembled
- **DST edge cases:** POSIX tz strings handle DST automatically via `tzset()` but verify your timezone string at [timezone.ctz.io](https://timezone.ctz.io)
- **Multi-day tasks:** Current scheduler is wall-clock daily only; overnight tasks (spanning midnight) are not supported in v1
- **Touch calibration:** XPT2046 may need calibration constants adjusted per panel; use TFT_eSPI's `touch_calibrate` example sketch first run
- **Weather backoff on reboot:** `wxBackoffUntil` is stored in RAM only — if the device reboots while in a permanent backoff state (bad key/city), it will attempt one fetch on boot before re-entering backoff. This is fine in practice since the error is surfaced immediately on that first attempt.

---

## License

MIT — build, mod, and share freely.
