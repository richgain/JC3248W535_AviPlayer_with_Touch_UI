# JC3248W535_AviPlayer_with_Touch_UI

## Background
This project was designed to be built into an old wooden box to make a magical video player for a 3 year-old child's favourite program, Bagpuss. 
![IMG_7144](https://github.com/user-attachments/assets/57fcb7d0-bff6-499d-a523-9ad16d8d49e7)

The basic version has been working for the last year and is much loved, but simply shows all thirteen episodes in a completely random order. This new project adds touchscreen controls allowing the child to choose which of the  episodes to play.
![IMG_7142](https://github.com/user-attachments/assets/89ed225c-7882-4991-b9c2-6433fc6eee25)



## Hardware
The JC3248W535 is a great little ESP32-S3 powered board that includes a touch screen, audio amplifier, SD card slot and battery management — all in one self-contained unit at a very reasonable price. Here I describe how I managed to create an AVI player with a touch screen UI using the new JC3248W535-Driver and the ArduinoGFX library. 

## Challenges
The JC3248W535 uses an AXS15231 chip to control the display and the touchscreen. It is notoriously difficult to get the touch working with the available drivers.

The JC3248W535 is designed to be operated in portrait mode (320 x 480 pixels), but for this project I wanted to show a TV-like display, i.e. landscape mode (480 x 320 pixels). Rotating the AVI 90 degrees during playback is possible but drastically reduces the framerate achievable by the ESP32S3. The solution is to rotate the source file 90 degrees during the conversion process, allowing the processor to display a 320x480 video at full-speed.

## Credits
The project was inspired by [@moononournation](https://github.com/moononournation)'s remarkable work on the [Arduino_GFX library](https://github.com/moononournation/Arduino_GFX) and his [AVI player example](https://github.com/moononournation/aviPlayer), which demonstrated that smooth video playback on an ESP32 is possible. Another project by [thelastoutpostworkshop](https://github.com/thelastoutpostworkshop/JC4827W543_avi_player) used a different device but provided the inspiration for the player controls. The touch driver is provided by the excellent [JC3248W535-Driver](https://github.com/me-processware/JC3248W535-Driver) repository by me-processware, whose README documentation is itself worth reading.

---

## Prerequisites

### Board Manager
In the Boards menu, install and select version **3.0.2** of **"esp32" by Espressif Systems**.

### Library Manager
Install version **1.4.9** of **"GFX Library for Arduino"** by Moon On Our Nation.

### Manual Library Installs
The following libraries must be installed manually:

- https://github.com/pschatzmann/arduino-libhelix
- https://github.com/arionik/avilib
- https://github.com/esp-arduino-libs/ESP32_JPEG
- https://github.com/me-processware/JC3248W535-Driver

---

## Arduino IDE Settings

Under **Tools**, select the following settings:

| Setting | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| Port | Your board's COM port |
| USB CDC On Boot | Enabled |
| Flash Mode | QIO 120 MHz |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| PSRAM | OPI PSRAM |

> **Note:** PSRAM is **required** for the canvas frame buffer. The sketch will print an error on startup if PSRAM is not correctly enabled.

---

## Creating the videos

You will need to convert your videos using free software to avi files that will play on this device.

### Conversion Process
I used a 2-stage video file conversion process. 
- Step 1. Use the free HandBrake program to scale your source video file to 480 pixels by 320 pixels AND rotate it 90 degrees clockwise.
- Step 2. Use ffmpeg as decribed by @moononournation to convert to the Cinepak-compatible AVI file.

### SD Card Setup

Copy your AVI files to a folder called `/avi320x480` on the SD card. The player expects files in 320x480 portrait format.

---

## Known Issues and Gotchas

These are the problems encountered during development that cost the most time to resolve. Documenting them here may save others significant debugging effort.

### 1. Touch Driver
The touch driver included in the ArduinoGFX library does **not** work with the AXS15231B controller on this board. This was the primary motivation for this project. The solution is the [JC3248W535-Driver](https://github.com/me-processware/JC3248W535-Driver) by me-processware, which works correctly out of the box.

### 2. Big Endian Pixel Flag in cinepak.h
Without this fix, video colours will appear severely distorted — greens, purples and cyans instead of natural colours. If you see distorted colours, change the commented/uncommented state of the Big Endian pixel flag in `cinepak.h` at line 25, to  :

```cpp
// Change this:
// #define BIG_ENDIAN_PIXEL

// To this:
#define BIG_ENDIAN_PIXEL

or vice versa.
```

### 3. Canvas vs Direct Display Driver
This project uses an `Arduino_Canvas` buffer (required for the QSPI interface), which means:
- All drawing goes to the canvas buffer first
- `display.flush()` must be called to push the canvas to the physical display
- `output_buf` must be obtained via `gfx->getFramebuffer()` on the canvas, not the display driver
- The `avi_draw()` function must call `flush()` to display the contents of the screen buffer.


### 4. setTextSize() Must Be Called Before getTextBounds()
When centring text on screen, `getTextBounds()` measures text at the **current** text size. If `setTextSize()` is called after `getTextBounds()`, the measurement will be wrong and the text will appear off-centre. Always set the text size first:

```cpp
gfx->setTextSize(3);   // Set size FIRST
gfx->getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);  // THEN measure
int titleX = (regionW - textW) / 2 - x1;
int titleY = regionY + (regionH - textH) / 2 - y1;
```

### 5. Correct Text Centring Formula
The correct formula to centre text both horizontally and vertically within a defined region, accounting for the `x1`/`y1` offsets returned by `getTextBounds()`, is:

```cpp
int titleX = (REGION_WIDTH - textW) / 2 - x1;
int titleY = REGION_Y + (REGION_HEIGHT - textH) / 2 - y1;
```

Note: `y1` from `getTextBounds()` is typically a small negative number representing the ascender height above the baseline. Including `- y1` gives true visual centring.

---

## Credits and Inspiration

This project stands on the shoulders of others' generous open source contributions:

- **[Volos Projects](https://www.youtube.com/@VolosProjects)** — YouTube channel that introduced many people (including this author) to ESP32 development
- **[@moononournation](https://github.com/moononournation)** — Author of the Arduino_GFX library and the AVI player example that forms the foundation of this project
- **[me-processware](https://github.com/me-processware/JC3248W535-Driver)** — Author of the JC3248W535 touch driver that made this project possible, and whose README documentation sets a high standard

---

## License

MIT
