# RFID Digital Jukebox with M5Stack CoreS3

An interactive, physical-media music player and cataloging system built around the ESP32-S3 (**M5Stack CoreS3 SE**). The device reads passive RFID/NFC album cards, renders high-resolution album artwork on the integrated LCD screen from internal storage, and plays high-fidelity audio via a DFPlayer Mini decoder paired with a PAM8403 Class-D stereo amplifier.

Designed for both personal home audio setups and in-store interactive cataloging in vinyl/record shops.

---

## 🌟 Key Features

* **Physical Media Experience:** Tap an RFID card/tag to instantly load and play an album.
* **Dual-MicroSD Architecture:** 
  * **Internal SPI MicroSD:** Stores album cover artwork (`.jpg` baseline format).
  * **Dedicated Audio MicroSD:** Stores high-quality MP3 tracks managed by the DFPlayer Mini hardware decoder.
* **Active Power Management:** Programmatically controls the internal AXP2101 PMIC to deliver stable 5V rail power (`VEXT`) to external modules.
* **Instant Boot & Low Power:** Powered by ESP32-S3 RTOS, eliminating the slow boot times of single-board computers (e.g., Raspberry Pi).
* **On-Screen Album History:** Touchscreen interface allowing users to browse recently played albums.

---

## 🛠️ Hardware Requirements & Components

| Component | Function | Connection / Protocol |
| :--- | :--- | :--- |
| **M5Stack CoreS3 SE** | Main ESP32-S3 MCU & Capacitive Touch Display | Master Controller (AXP2101 PMIC) |
| **MFRC522** | 13.56 MHz RFID/NFC Card Reader | Shared SPI Bus |
| **DFPlayer Mini** | Hardware MP3 Decoder & SD Audio Player | Hardware UART (Port C - 9600 Baud) |
| **PAM8403** | 2x3W Class-D Stereo Audio Amplifier | Direct DAC Line Input (`DAC_L`, `DAC_R`) |
| **Internal MicroSD** | High-speed storage for album covers (`.jpg`) | SPI Bus (FAT32) |
| **Audio MicroSD** | Music track storage (`.mp3` inside `/mp3`) | FAT32 |
| **Passive RFID Cards** | 13.56 MHz ISO14443A tags representing albums | Non-contact reading |

---

## 🔌 Circuit & Hardware Pinout

> **Note on Voltage Levels:** The ESP32-S3 operates natively at **3.3V logic**. Do **NOT** place a 1kΩ resistor on the UART TX line to the DFPlayer Mini, as it lowers signal voltage below the logic high threshold.

### 1. Power Distribution (Port A - Red Grove Port)
* **5V Output (`VEXT` / `+`)** → `VCC` (Pin 1) of DFPlayer Mini **&** `5V/VCC` of PAM8403
* **GND (`-`)** → `GND` of DFPlayer Mini **&** `GND` of PAM8403

### 2. Hardware UART Communication (Port C - Blue Grove Port)
* **GPIO 17 (ESP32 TX)** → `RX` (Pin 2) of DFPlayer Mini (Direct 3.3V connection)
* **GPIO 18 (ESP32 RX)** → `TX` (Pin 3) of DFPlayer Mini

### 3. Shared SPI Bus (RFID Reader & Internal MicroSD)
* **SCK:** GPIO 36
* **MISO:** GPIO 35
* **MOSI:** GPIO 37
* **CS (Internal MicroSD):** GPIO 4
* **CS (MFRC522 RFID):** GPIO 8

### 4. Analog Audio Routing
* **DFPlayer (`DAC_L` / `DAC_R`)** → **L / R** Inputs of PAM8403 Amplifier
* **DFPlayer (`GND`)** → Audio **GND** Input of PAM8403
* **PAM8403 (`L+`/`L-` and `R+`/`R-`)** → Independent Left & Right Speaker Output Channels

---

## 📁 SD Card Directory Structure

To ensure compatibility with the `playMp3Folder()` command, format the audio MicroSD card to **FAT32** and organize files inside a root folder named `mp3`:

```text
sd_card/
└── mp3/
    ├── 0001_el_camino_del_exceso.mp3
    ├── 0002_el_club_de_los_imposibles.mp3
    ├── 0006_the_long_and_winding_road.mp3
    └── 0007_breezin.mp3
```

---

## 🚀 Software Setup & Installation

### Required Libraries
Install the following libraries via the Arduino IDE Library Manager or PlatformIO:
1. **M5Unified** (by M5Stack)
2. **MFRC522** (by GithubCommunity)
3. **DFRobotDFPlayerMini** (by DFRobot)

### PMIC Power Initialization
Ensure your `setup()` includes the power management command to enable the 5V output on Port A:

```cpp
#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Enable 5V power rail (VEXT) on Grove Port A for external hardware
  M5.Power.setVext(true);
  
  // Proceed with SPI and UART initialization...
}
```

---

## 📄 License

Distributed under the **MIT License**. See `LICENSE` for more information.
