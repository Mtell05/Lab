# ESP32-S3 Round Display Avatar Demo

Minimal PlatformIO project for ESP32-S3 DevKitC-1 driving a GC9A01 round SPI display and a MAX98357A I2S amplifier while connecting to Wi-Fi.

## Pin Map

| Function            | ESP32-S3 Pin | Peripheral Pin |
| ------------------- | ------------ | -------------- |
| GC9A01 MOSI         | 11           | DIN            |
| GC9A01 SCK          | 12           | CLK            |
| GC9A01 CS           | 10           | CS             |
| GC9A01 DC           | 9            | D/C            |
| GC9A01 RST          | 13           | RST            |
| GC9A01 Backlight    | 21           | LED            |
| MAX98357A BCLK      | 16           | BCLK           |
| MAX98357A LRCLK     | 17           | LRC            |
| MAX98357A DIN       | 15           | DIN            |
| MAX98357A SD (mute) | 18           | SD             |
| 3V3 / GND           | 3V3, GND     | VCC, GND       |

> ⚠️ Disconnect power before wiring. Keep I2S lines short (<10 cm) and twist with ground if possible to reduce noise.

## Build & Flash

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html) or the VS Code PlatformIO extension.
2. Edit `include/secrets.h` and fill in `WIFI_SSID`/`WIFI_PASS`.
3. Connect the ESP32-S3 board via USB.
4. From this folder run:
   ```bash
   pio run -t upload
   pio device monitor
   ```

## Quick Test

On boot you should see:

* Serial log showing Wi-Fi connection status and IP address.
* Round face avatar drawn on the GC9A01 display with blinking eyes.
* A short 440 Hz tone played once through the MAX98357A amplifier.

If Wi-Fi fails, the avatar background turns red and a warning is printed over serial.

## Dependencies

* Arduino framework for ESP32 (via PlatformIO `espressif32` platform 6.5.0).
* `Adafruit GC9A01A` + `Adafruit GFX` libraries (auto-installed by PlatformIO).

## Debug Checklist

* Verify 3V3 rail can source peak current (~500 mA with Wi-Fi + backlight).
* Tie MAX98357A SD pin HIGH (GPIO18 set high in firmware).
* Ensure GC9A01 backlight has a series resistor if required by your module.
* For brownouts, power via a stable 5 V supply; avoid powering heavy speakers directly from USB.
