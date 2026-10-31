# SpoolmanScale – Build Guide

This guide covers everything you need to physically build a SpoolmanScale: parts, wiring, assembly, and calibration.

For the project overview, features, and flashing instructions, see the [README](README.md).

> ⚠️ This is a beta build guide. If something is unclear or doesn't fit your hardware batch, please report it on [MakerWorld](https://makerworld.com/@FormFollowsF) or in the [Discord](https://discord.gg/K3KmKnrWhP).

---

## Parts List

### Electronics

| Component | Model | Link |
|---|---|---|
| MCU + Display | WT32-SC01 Plus (ESP32-S3, 480×320, ST7796) | [AliExpress](https://a.aliexpress.com/_Ey1VKfI) |
| Debug Board (recommended) | ZXACC-ESPDB | [AliExpress](https://a.aliexpress.com/_Eu5Y0Ug) |
| NFC Reader | PN532 | [AliExpress](https://a.aliexpress.com/_ExScN8M) |
| Scale ADC | NAU7802 (Adafruit) | [AliExpress](https://a.aliexpress.com/_EvlFNj2) |
| Load Cell | YZC-133 2 kg beam cell (5 kg works too) | [AliExpress](https://a.aliexpress.com/_EuhhVF2) |
| Connector Cables | STEMMA QT / JST cables | [AliExpress](https://a.aliexpress.com/_Ezjg6fQ) |
| Connector Cables (recommended) | Micro JST 1.0 SH 5-pin – easier assembly | [Amazon](https://amzn.eu/d/0aKJ4Va9) |
| USB-C Panel Mount 90° | 30 cm, full USB-C PD + data support | [AliExpress](https://a.aliexpress.com/_EjQ6sma) |

### Additional Materials

- Thin stranded wire in 5 different colors (black, red, yellow, white, green – ~30–40 cm each)
- 2× M5×25 socket head screws
- 2× M4×15 socket head screws
- 9× M2.5×5 self-tapping screws
- 2–4× M2×4.4 self-tapping screws ([example](https://a.aliexpress.com/_EyCD3rS))

Self-tapping screws are recommended, but standard machine screws (M2.5×5, M2×4) will likely work as well.

---

## 3D Files

The printable enclosure is available on MakerWorld:
👉 [makerworld.com/@FormFollowsF](https://makerworld.com/de/models/2713675-spoolmanscale#profileId-3005075)

All parts fit on 3 print plates (currently distributed across 4). If you can print TPU, use it for the feet – they grip better. Otherwise, self-adhesive silicone feet work just as well.

> If the enclosure is too tight or too loose for your display, please report it – display dimensions can vary slightly between batches.

---

## Assembly Steps

### Step 1 – Flash the board first

Before assembling anything, flash the firmware to the bare board via the [Web Flasher](https://niko11111.github.io/SpoolmanScale). This confirms the board is working before you start wiring.

### Step 2 – Wire components one at a time

Connect components one at a time – NFC reader first, then the scale ADC and load cell – and verify each one works before moving on. Much easier to debug at this stage than inside a fully assembled enclosure.

See the wiring tables below for all connections.

### Step 3 – Assemble & close up

Wire everything up, do a final check that all components are working, then press the display into the enclosure. It should sit snugly without screws. If you want extra security, it can be fastened from the back.

### Step 4 – Calibrate the scale

Go to **Settings → Scale → Calibration**.

1. Tare the scale with nothing on it
2. Place an object with a known weight – ideally ~1000 g (a full filament spool verified on a kitchen scale works well)
3. Enter the exact weight in grams and save

The more precise your reference weight, the more accurate your results.

---

## Wiring

All components connect to the **I/O connector** on the WT32-SC01 Plus using the included 7-pin cable. The I/O socket has 8 pins - plug the 7-pin cable in flush to the left.

The PN532 and NAU7802 share the same I2C bus (SDA/SCL) and are wired in parallel. **Do not daisy-chain them via the NAU7802's STEMMA QT passthrough** – that port only supplies 3.3 V and the PN532 requires 5 V.

### WT32-SC01 Plus I/O Connector

| Pin | Color | Signal | Connect to |
|---|---|---|---|
| 1 | Red | 5 V | PN532 VCC + NAU7802 VIN |
| 2 | Black | GND | PN532 GND + NAU7802 GND |
| 3 | Yellow | GPIO10 (SDA) | PN532 SDA + NAU7802 SDA |
| 4 | Green | GPIO11 (SCL) | PN532 SCL + NAU7802 SCL |
| 5 | Blue | - | unused |
| 6 | White | - | unused |
| 7 | Brown | GPIO14 | PN532 RSTPDN |

### PN532 – Soldering & Assembly

The PN532 has no connector - wires must be soldered directly. **Always solder from the back of the PCB** - there is not enough clearance on the front once it's mounted.

**Recommended assembly order:**

1. Solder all other components first (NAU7802, load cell, WT32 breakout cable) with the enclosure open
2. Leave the PN532 wires loose and long enough to work with
3. Feed the loose wires up through the PN532 mount opening from below
4. Solder the wires to the back of the PN532 PCB
5. Slide the PN532 into its mount and pull the excess cable back down into the enclosure
6. Route and secure cables so they don't press against the weighing platform

If you use a 5-pin JST SH 1.0mm connector on the PN532 pigtail, the mount opening is just large enough to pass the connector through - meaning you can do all soldering outside the enclosure and simply plug it in during final assembly.

### PN532 – Cable Pinout (JST SH 1.0mm 5-pin, 3rd party colors)

| Pin | Color (3rd party) | Signal | WT32 Cable |
|---|---|---|---|
| 1 | Black | GND | Pin 2 (Black) |
| 2 | Red | 5 V | Pin 1 (Red) |
| 3 | Yellow | SDA | Pin 3 (Yellow) |
| 4 | White | SCL | Pin 4 (Green) |
| 5 | Orange | RSTPDN | Pin 7 (Brown) |

Solder the orange wire to **RSTPDN**, the pin at the end of the labelled 10-pin row (AUX1 ... SIGIN, RSTPDN), not to the header opposite. The [manual](https://niko11111.github.io/SpoolmanScale-Docs/build/wiring/) shows it in a photo.

### NAU7802 – STEMMA QT Connector (JST SH 1.0mm 4-pin)

Soldering directly to the labeled pads (VIN, GND, SDA, SCL) on the NAU7802 is the safest option. If you prefer the STEMMA QT connector, note that the pin order of 3rd-party JST SH 1.0mm cables does **not** match the WT32 cable - you will need to re-pin or swap wires:

| STEMMA QT Pin | Color (3rd party) | Signal | WT32 Cable |
|---|---|---|---|
| 1 | Black | GND | Pin 2 (Black) |
| 2 | Red | 5 V | Pin 1 (Red) |
| 3 | Yellow | SDA | Pin 3 (Yellow) |
| 4 | White | SCL | Pin 4 (Green) |

### Load Cell → NAU7802

| Load Cell wire | NAU7802 terminal |
|---|---|
| Red | E+ (excitation +) |
| Black | E− (excitation −) |
| White | A+ (signal +) |
| Green | A− (signal −) |

> If the scale shows negative values, swap A+ and A−.

### USB-C Panel Mount

The USB-C panel mount extension needs to be trimmed before installation. Using a utility knife, carefully shorten the connector housing little by little until it no longer protrudes beyond the display edge. Take your time – small cuts at a time. Once flush, it will fit cleanly into the enclosure.

---

## Other Boards

The firmware is built for one board at a time. Everything above describes the WT32-SC01 Plus, the board the enclosure is designed for. The same firmware, with every feature, also runs on:

| Board | Environment | Display | Status |
|---|---|---|---|
| WT32-SC01 Plus | `wt32-sc01-plus` | 480×320, native | the reference board |
| Waveshare ESP32-S3-Touch-LCD-7 | `waveshare-s3-lcd7` | 800×480, the 480×320 UI enlarged to fill it | new, see the notes below |
| Guition JC3248W535C | `guition-jc3248w535c` | 320×480 turned to the UI's own 480×320 | first unit connected 28.10.2026 |

The Waveshare ESP32-S3-Touch-LCD-7 has its own printable enclosure, the [7" Spoolman T-Scale](https://makerworld.com/en/models/3429397-7-spoolman-t-scale-waveshare-esp32-s3-touch-7) on MakerWorld.

### Waveshare ESP32-S3-Touch-LCD-7

The 7" 800×480 board with the ESP32-S3-WROOM-1-N16R8 module (16 MB flash, 8 MB PSRAM). The 1024×600 "7B" is a different board and is not supported. The 4.3" sibling, the 4.3B, has the same panel pins but different connectors; see the [note at the end of this section](#the-43b-untested).

**What is different on this board**

- **The picture** is the same 480×320 UI, enlarged until it fills the 800×480 panel. That makes everything 11 % wider than on the WT32; every screen and every setting is otherwise identical.
- **Brightness** is applied to the picture, not to the backlight: the board can only switch its backlight on and off. Dimming and "screen off" work as on the WT32.
- **USB**: use the connector labelled **USB**, not the one labelled UART, for flashing, the serial log and the Web Flasher's WiFi setup.
- **SD card**: the slot on the board is used for the log, as on the WT32.

**Wiring**

The PN532 and the NAU7802 share one I2C bus, as on the WT32, on the board's **UART2 connector**. Set the **UART selection switch** next to it to **UART2**: in the UART1 position the board's USB to UART chip drives one of the two pins and the sensors do not answer.

| Signal | ESP32-S3 pin | Where on the board | Connect to |
|---|---|---|---|
| SDA | GPIO43 (TXD) | UART2 connector | PN532 SDA + NAU7802 SDA |
| SCL | GPIO44 (RXD) | UART2 connector | PN532 SCL + NAU7802 SCL |
| RST (optional) | GPIO6 | Sensor connector | PN532 RSTPD_N |
| GND | GND | either connector | PN532 GND + NAU7802 GND |
| Supply | 3V3 or 5 V | as labelled on the board | PN532 VCC + NAU7802 VIN |

Check the pin order against the labels printed on your board before connecting anything: it is not the order of the WT32 cable.

**Do not use the board's own I2C connector for the PN532.** That bus carries the board's I/O expander, which answers on the PN532's address (0x24). The reader would switch off the backlight and the SD card with every frame it is sent.

The load cell connects to the NAU7802 as described above.

**Flashing**

In the browser, with nothing to install: open the [Web Flasher](https://wolfrage76.github.io/Waveshare-Spoolman-Scale/#waveshare-s3-lcd7) in Chrome or Edge, with the board on its **USB** connector.

With PlatformIO, from the repository:

```
pio run -e waveshare-s3-lcd7 -t upload
```

Or flash a release without building: every release carries `SpoolmanScale-waveshare-s3-lcd7_<version>_factory.bin`, one file with bootloader, partition table and firmware, to be written at offset 0:

```
esptool --chip esp32s3 write-flash 0x0 SpoolmanScale-waveshare-s3-lcd7_<version>_factory.bin
```

If the board is not found: hold **BOOT**, press and release **RESET**, release **BOOT**, and try again.

Later updates come over WiFi as on the WT32. The board downloads its own image (`SpoolmanScale-waveshare-s3-lcd7.bin`), and the upload in the browser refuses an image built for another board.

#### The 4.3B (untested)

> **Warning:** nothing below has been tried on a board yet. The ESP32-S3-Touch-LCD-4.3B is not a supported board, the Web Flasher does not list it, and getting the sensors connected means removing two parts from it. This is a note for anyone who wants to try, worked out from the [schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3B/ESP32-S3-Touch-LCD-4.3B-Sch.pdf), not a build guide.

Same module, panel, touch, SD card and expander as the 7", so the `waveshare-s3-lcd7` image should drive its display without change. The connectors differ. The 4.3B has a 16-pin terminal block with an I2C connector, CAN, RS485 and isolated digital in/out, and nothing else: no UART2 connector, no UART selection switch, no sensor connector. Every ESP32-S3 pin that is free on the 7" is tied up here:

- GPIO43 and GPIO44, the 7"'s sensor bus, belong to the **RS485 transceiver** (SP3485, U7). GPIO43 is the transceiver's receiver output, actively driven whenever the bus is idle. GPIO44 switches the transceiver's direction through R68 (100 Ω) into the base of transistor S1; released, that base junction holds the net at about 0.75 V, so a pull-up alone never gets it high. Neither pin can carry I2C or UART as long as those parts are in place.
- GPIO15 and GPIO16 drive the CAN transceiver, GPIO6 is the RTC's interrupt, GPIO19 and GPIO20 are USB. None of them is on a connector.
- The **I2C connector** (D_SDA, D_SCL on the terminal block) is the board's own bus, GPIO8 and GPIO9, with the CH422G on it. The CH422G answers on 0x24, the PN532's address, and takes every byte of a PN532 frame as a mode write: the panel and the touch would be reset and the backlight and SD card switched off. The PN532 cannot go on this bus. The NAU7802 (0x2A) could, but the firmware does not read the scale on this bus.

**The modification: free GPIO43 and GPIO44 from the RS485 circuit**

See the RS485 block of the schematic. Two parts have to go. Afterwards the two pins are exactly the 7"'s sensor bus, both pulled up to 3.3 V with 4.7 kΩ (R64 on GPIO43, R67 on GPIO44), and the unmodified `waveshare-s3-lcd7` image should run the scale.

1. **Disconnect RO from GPIO43:** lift pin 1 of U7, the SP3485 next to the RS485 terminals, or remove U7 entirely. This frees SDA.
2. **Disconnect S1 from GPIO44:** remove R68, the 100 Ω resistor between the GPIO44 net and the base of S1 (or remove S1). Without this SCL sits at 0.75 V and never reads as high, whatever pull-up is added. This frees SCL.
3. Solder SDA and SCL to the two pins. The easiest pads are the module's castellations: **pin 37 (TXD0) is GPIO43 = SDA**, **pin 36 (RXD0) is GPIO44 = SCL**. The pad of U7 pin 1 (GPIO43) and the GPIO44-side pad of R68 or R67 carry the same nets.
4. Power and ground from the terminal block: `I2C_VCC` (3.3 V or 5 V, chosen by a resistor on the board, 3.3 V as shipped) and `GND`.

RS485 is dead after this; nothing else on the board uses it.

| Signal | ESP32-S3 pin | Where on the board | Connect to |
|---|---|---|---|
| SDA | GPIO43 | module pin 37 (TXD0), or U7 pad 1 | PN532 SDA + NAU7802 SDA |
| SCL | GPIO44 | module pin 36 (RXD0), or the R68/R67 pad | PN532 SCL + NAU7802 SCL |
| GND | GND | terminal block | PN532 GND + NAU7802 GND |
| Supply | I2C_VCC | terminal block | PN532 VCC + NAU7802 VIN |

The PN532's reset line (GPIO6 on the 7") has no pad on this board. Leave RSTPD_N unconnected; the firmware copes.

**Not implemented: PN532 over RS485**

The only path without soldering would put the NAU7802 on the board's I2C connector and talk to the PN532 in its UART (HSU) mode through the RS485 terminals, with a TTL-to-RS485 converter at the reader. That needs a UART transport for the PN532 (the Adafruit library is I2C and SPI only) and a scale driver that shares the display's I2C bus. Neither exists yet.

**Flashing** is as for the 7", with the USB-C connector: the Web Flasher's Waveshare 7" entry, or `pio run -e waveshare-s3-lcd7 -t upload`. There is a printable [4.3" Spoolman T-Scale](https://makerworld.com/en/models/3500125-4-3-spoolman-t-scale-waveshare-esp32-s3-43b) enclosure.

### Guition JC3248W535C

The capacitive 3.5 inch, marked JC3248W535C_I_Y or JC3248W535C_L_I_Y: ESP32-S3-WROOM-1-N16R8 (16 MB flash, 8 MB octal PSRAM), AXS15231B display and touch. The glass is 320×480 in portrait. The firmware turns it to 480×320, so the UI is the same size as on the WT32.

**What is different on this board**

- **The picture** is the WT32's 480×320 UI, turned to fit. Brightness is a real PWM on the backlight, as on the WT32.
- **USB**: the USB-C port, for flashing, the serial log and the Web Flasher.
- **SD card**: the socket on the board is used for the log, as on the WT32.
- **If the picture is upside down**, picture and touch together, switch `LANDSCAPE_TURN` in `src/boards/guition_jc3248w535c/panel.cpp` from 0 to 1 and build again. Scrambled colours, or red and blue exchanged, are the two knobs next to it, `WIRE_SWAPS_BYTES` and `MADCTL`.

**Wiring**

The PN532 and the NAU7802 share one I2C bus, on the board's **I2C connector**. It is brought out twice, a 1.25 mm JST and a 1.0 mm Qwiic plug, wired the same way. Pin 1 is GND, pin 2 is 3.3 V, pin 3 is SDA, pin 4 is SCL.

| Signal | ESP32-S3 pin | Where on the board | Connect to |
|---|---|---|---|
| SDA | GPIO17 | I2C connector, pin 3 | PN532 SDA + NAU7802 SDA |
| SCL | GPIO18 | I2C connector, pin 4 | PN532 SCL + NAU7802 SCL |
| RST (optional) | GPIO6 | 8 pin header, IO6 | PN532 RSTPD_N |
| GND | GND | I2C connector, pin 1 | PN532 GND + NAU7802 GND |
| Supply | 3.3 V | I2C connector, pin 2 | PN532 VCC + NAU7802 VIN |

Leave **IO5 (GPIO5)** alone: it is the battery voltage divider. Leave **IO46 (GPIO46)** alone: it is a strapping pin.

**Flashing**

In the browser, with nothing to install: open the [Web Flasher](https://wolfrage76.github.io/Waveshare-Spoolman-Scale/#guition-jc3248w535c) in Chrome or Edge, with the board on its USB-C port.

With PlatformIO, from the repository, board on the USB-C port:

```
pio run -e guition-jc3248w535c -t upload
```

Or flash a release without building: every release carries `SpoolmanScale-guition-jc3248w535c_<version>_factory.bin`, one file with bootloader, partition table and firmware, to be written at offset 0:

```
esptool --chip esp32s3 write-flash 0x0 SpoolmanScale-guition-jc3248w535c_<version>_factory.bin
```

If the board is not found: hold **BOOT**, press and release **RESET**, release **BOOT**, and try again.

Later updates come over WiFi. The board downloads its own image (`SpoolmanScale-guition-jc3248w535c.bin`), and the upload in the browser refuses an image built for another board.

### Adding a board

A board is a folder under `src/boards/` and an environment in `platformio.ini`. Nothing outside that folder names a board.

1. Copy the folder of the board that is closest to yours and rename it.
2. `board_config.h`: its name, its pins, and what its hardware can do. `src/boards/board.h` lists what the file has to define.
3. `panel.cpp`: display, touch and backlight, behind the functions of `src/hardware/panel.h`. A panel larger than 480×320 enlarges the picture there; the UI does not change.
4. Add the board to `src/boards/board.h` and an `[env:<board>]` to `platformio.ini` that defines its `BOARD_*` macro and compiles its folder.
5. Add the environment's name to the build matrix in `.github/workflows/ci.yml` and to `OTHER_BOARDS` in `.github/workflows/build-all-boards.yml`.

---

## Assembly Photos

[![](images/spoolmanscale_assembly_1.jpeg)](images/spoolmanscale_assembly_1.jpeg) [![](images/spoolmanscale_assembly_2.jpeg)](images/spoolmanscale_assembly_2.jpeg)
[![](images/spoolmanscale_assembly_3.jpeg)](images/spoolmanscale_assembly_3.jpeg) [![](images/spoolmanscale_assembly_4.jpeg)](images/spoolmanscale_assembly_4.jpeg)
[![](images/spoolmanscale_assembly_5.jpeg)](images/spoolmanscale_assembly_5.jpeg) [![](images/spoolmanscale_assembly_6.jpeg)](images/spoolmanscale_assembly_6.jpeg)
[![](images/spoolmanscale_assembly_7.jpeg)](images/spoolmanscale_assembly_7.jpeg) [![](images/spoolmanscale_assembly_8.jpeg)](images/spoolmanscale_assembly_8.jpeg)
[![](images/spoolmanscale_assembly_9.jpeg)](images/spoolmanscale_assembly_9.jpeg)
