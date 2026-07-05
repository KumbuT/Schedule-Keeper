# ⏳ XIAO ESP32-C3 Smart Task Countdown Timer

An advanced, battery-powered task countdown timer built using the Seeed Studio XIAO ESP32-C3 and LovyanGFX display engine. Features real-time state synchronization via WebSockets, an automated captive Wi-Fi portal, internal LittleFS-based flash storage, a custom Tailwind CSS configuration utility, and Deep Sleep power management.

---

## 🛠️ Hardware Specifications & Bill of Materials

* **Microcontroller:** Seeed Studio XIAO ESP32-C3
* **Display:** 2.8" TFT LCD Display with ILI9341 Driver (240 × 320 Resolution, SPI Interface)
* **Power Source:** 3.7V LiPo Battery (e.g., 500mAh - 1000mAh)
* **Alert System:** 5V Passive Buzzer
* **Control Inputs:** 2x Momentary Push Buttons (Next / Pause)
* **Passive Electronics:** 2x 200 kΩ Resistors (for battery voltage measurement divider)

---

## 🔌 Circuit Schematic Diagrams

### 1. Unified Wiring Connections
Connect your components to the Seeed Studio XIAO pins according to this layout map:


---

### Updated Touch Screen Device Wiring Configuration Layout
Both display logic vectors and input coordinates parse across a single unified SPI bus architecture channel:

          +---------------------------------------+

          |         Seeed Studio XIAO C3          |
          |                                       |
          |  [D0/A0] <--- Batt Voltage Divider    |
          |  [D1]    ---> Screen Chip Select (CS) |
          |  [D2]    ---> Data/Command (DC)       |
          |  [D3]    ---> Screen Reset (RST)      |
          |  [D4]    ---> Passive Buzzer (+)      |
          |  [D5]    ---> Touch Chip Select (T_CS)|
          |  [D8]    ---> Shared SPI SCK          |
          |  [D9]    <--- Shared SPI Touch MISO   |
          |  [D10]   ---> Shared SPI MOSI / T_DIN |
          +---------------------------------------+
--- 

### 2. External Analog Battery Monitor Divider
Because the XIAO C3 lacks an internal battery measurement routing connection trace to its onboard ADC pins, you must build an external voltage divider network across the battery rails to sample state levels safely through pin `D0`:

```text
LiPo Battery (+) ───────[ 200k Ohm ]───────┬───────> XIAO D0 / A0
                                           │
                                       [ 200k Ohm ]
                                           │
GND / LiPo (-)   ──────────────────────────┴───────> XIAO GND
```
*Note: This splits a peak 4.2V battery charge cleanly down into a safe 2.1V maximum reading threshold, protecting the analog sampling pin.*

### 3. Tactile Control Inputs & Alarm Alerts
Buttons rely on internal internal pull-up configurations. Wire them straight down to ground lines:

```text
XIAO D5 (Next)  ───────[ Button 1 ]───────> GND
XIAO D6 (Pause) ───────[ Button 2 ]───────> GND

XIAO D4 (Buzzer) ───────[ Passive Buzzer (+) ] ... [ Passive Buzzer (-) ] ──────> GND
```

---

## 📁 Repository Directory Structure

```text
TaskTimer/
├── data/
│   └── index.html           # Tailwind CSS Frontend App Asset
├── src/
│   └── main.cpp             # Core Firmware Logic & LovyanGFX Driver
└── platformio.ini           # Locked Project Dependency Declarations
```

---

## 🚀 Deployment Instructions

### 1. Compile and Flash Firmware
Open your project terminal directory inside PlatformIO and load the device core code block:
```bash
pio run --target upload
```

### 2. Upload Web UI Assets to Flash Memory
Compile and load the HTML asset payload across into your chip's integrated `LittleFS` memory segment partition workspace:
```bash
pio run --target uploadfs
```

---

## 📶 Operation & Initial Configurations

1. **First-Time Boot:** On initial deployment, the device generates a local captive Wi-Fi Access Point named `TaskTimer-Setup`. Connect your computer or phone browser directly to it to bind the timer to your home network.
2. **Accessing Configuration Interface:** Find the dynamically allocated IP address displayed locally across your device screen. Navigate directly into that index path location (e.g., `http://1.xx`) to manage, edit, add, or sort tasks with the interactive builder.
3. **Hardware Navigation Hooks:**
   * **Pause Button (D6):** Toggles countdown loops on and off.
   * **Next Button (D5):** Skips the current running sequence, advancing down into the next queued step.
   * **Deep Sleep Wakeup:** If left unmanaged or idle for 5 continuous minutes, the microcontroller slips down into low-draw Deep Sleep state parameters. Pressing the **Next Button (D5)** triggers a hardware interrupt wakeup loop to restore operations instantly.
