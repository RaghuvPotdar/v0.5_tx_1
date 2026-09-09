Production firmware for the RAVEN TX simulator transmitter (ESP32-S3, native USB HID joystick).
Full design rationale is in `../RAVEN_TX_Handbook.md` (Sections 9–13).

## Build & flash (PlatformIO)

```bash
# from this firmware/ directory
pio run                     # build
pio run -t upload           # build + flash over USB-C
pio device monitor          # 115200 baud debug log (USB CDC)
```

Arduino IDE 2.x alternative: install the **esp32 by Espressif** board package, select
*ESP32S3 Dev Module*, set **USB Mode = "USB-OTG (TinyUSB)"**, **USB CDC On Boot = Enabled**,
**Flash 16MB**, **PSRAM = OPI PSRAM**, then open `src/main.cpp` as a sketch (rename to `.ino`
or use the PlatformIO project as-is).

## Critical build flags (already in `platformio.ini`)

| Flag | Value | Why |
|------|-------|-----|
| `ARDUINO_USB_MODE` | `0` | USB-OTG / TinyUSB — REQUIRED for custom HID. `1` breaks HID. |
| `ARDUINO_USB_CDC_ON_BOOT` | `1` | Debug serial over USB CDC (optional). |
| `RAVEN_DISPLAY_SH1106` | `1` | `1` = 1.3" SH1106, `0` = 0.96" SSD1306. |

## First-bring-up order (see Handbook Section 18 spikes)

1. Flash, confirm Windows shows **"RAVEN TX"** under *Set up USB game controllers*.
2. Open the controller properties; move sticks — 6 axes respond, switches toggle 10 buttons.
3. Enter **Menu → Calibrate Sticks**: center, then sweep all controls; save.
4. Verify in Velocidrone (recommended) + one other sim.

## Module map

| File | Responsibility |
|------|----------------|
| `include/config.h` | Pin map (authoritative), types, versions |
| `src/main.cpp` | Init, dual-core tasks, control loop |
| `src/hid_joystick.cpp` | USB HID descriptor + report send |
| `src/inputs.cpp` | ADC oversample/filter, switch debounce |
| `src/calibration.cpp` | Calibration math + interactive capture |
| `src/storage.cpp` | NVS persistence (CRC + version) |
| `src/battery.cpp` | VBAT measure, %, charge/USB status |
| `src/display_ui.cpp` | OLED screens + menu/cal FSM |
| `src/diagnostics.cpp` | Self-test + serial telemetry |

## Calibration data
Stored in NVS namespace `ravtx`, key `cal`, as a CRC-checked, versioned blob — survives
reflash unless NVS is erased (`pio run -t erase`). One firmware image for all units (A14).
