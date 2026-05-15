# ESP32 Automatic Blinds (PlatformIO)

This project controls two blinds (`left` and `right`) using an ESP32 and two 28BYJ-48 stepper motors (through ULN2003 drivers). It provides a web UI for manual control, calibration, Wi-Fi setup, and weekday schedules.

## Hardware requirements
- ESP32 development board
- 2x 28BYJ-48 stepper motors
- 2x ULN2003 driver boards
- Stable power supply suitable for motors + ESP32

## Features
- Two motor control (left/right)
- Calibration for up and closed positions
- Position and settings persistence (ESP32 Preferences)
- Startup setup AP for 5 minutes
- Optional permanent self-hosted AP mode
- Web UI on port 80
- Weekday open/close schedules
- Berlin/Germany timezone via NTP
- Current motor position persistence

## Important limitation
28BYJ-48 steppers have **no position feedback**. Saved current positions are only reliable if blinds/motors were **not moved while ESP32 power was off**.

## Project layout

```
automatic-blinds/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── BlindMotor.cpp
│   ├── Storage.cpp
│   ├── WifiManager.cpp
│   ├── WebUi.cpp
│   └── Scheduler.cpp
├── include/
│   ├── Config.h
│   ├── BlindMotor.h
│   ├── Storage.h
│   ├── WifiManager.h
│   ├── WebUi.h
│   └── Scheduler.h
└── README.md
```

## Pin mapping
Left motor:
- IN1 = GPIO 4
- IN2 = GPIO 18
- IN3 = GPIO 19
- IN4 = GPIO 21

Right motor:
- IN1 = GPIO 27
- IN2 = GPIO 26
- IN3 = GPIO 25
- IN4 = GPIO 33

`AccelStepper` pin order is preserved as: **IN1, IN3, IN2, IN4**.

## Install PlatformIO CLI
1. Install Python (3.x).
2. Install PlatformIO:
   ```bash
   python -m pip install platformio
   ```

## Build / upload / monitor
- Build:
  ```bash
  pio run
  ```
- Upload:
  ```bash
  pio run --target upload
  ```
- Serial monitor:
  ```bash
  pio device monitor
  ```

## First use
After flashing, ESP32 starts the setup AP:
- SSID: `Automatic-Blinds-Setup`
- Password: `blinds1234`

Then:
1. Connect to that Wi-Fi network.
2. Open `http://192.168.4.1`.
3. Configure home Wi-Fi or keep self-hosted mode enabled.
4. If using home Wi-Fi, find ESP32 IP from:
   - router device/client list
   - PlatformIO serial monitor output
   - network scan tool
5. Open that IP in a browser.
