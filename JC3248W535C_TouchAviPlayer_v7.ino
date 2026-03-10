// WORKING

/*******************************************************************************
 * AVI Player example
 *
 * Dependent libraries:
 * Arduino_GFX: https://github.com/moononournation/Arduino_GFX.git
 * avilib: https://github.com/lanyou1900/avilib.git
 * libhelix: https://github.com/pschatzmann/arduino-libhelix.git
 * ESP32_JPEG: https://github.com/esp-arduino-libs/ESP32_JPEG.git
 *
 * Setup steps:
 * 1. Change your LCD parameters in Arduino_GFX setting
 * 2. Upload AVI file
 *   FFat/LittleFS:
 *     upload FFat (FatFS) data with ESP32 Sketch Data Upload:
 *     ESP32: https://github.com/lorol/arduino-esp32fs-plugin
 *   SD:
 *     Copy files to SD card
 *
 * Video Format:
 * code "cvid": Cinepak
 * cod "MJPG": MJPEG
 *
 * Audio Format:
 * code 1: PCM
 * code 85: MP3
 ******************************************************************************/
const char *root = "/root";
const char *avi_folder = "/avi320x480";

// #ifdef CANVAS_R1
// const char *avi_folder = "/avi480x320";
// #else
// const char *avi_folder = "/avi320x480";
// #endif


#include <JC3248W535.h>

// Include audio and SD card pin definitions

float gainLevel = 0.5f;
int volumeLevel = 5; // Default 5 bars (0.5 gain)
bool autoMode = false;         // Auto play mode flag
unsigned long playPressStart = 0; // For long press detection
bool volButtonPressed = false;

// I2S
// #define I2S_DEFAULT_GAIN_LEVEL
#define I2S_OUTPUT_NUM I2S_NUM_0
#define I2S_MCLK -1
#define I2S_BCLK 42
#define I2S_LRCK 2
#define I2S_DOUT 41
#define I2S_DIN -1

// #define AUDIO_MUTE_PIN 48   // LOW for mute


// SD card
#define SD_SCK 12
#define SD_MOSI 11 // CMD
#define SD_MISO 13 // D0
// #define SD_D1 1
// #define SD_D2 6
#define SD_CS 10 // D3

#include <U8g2lib.h>
#include <Preferences.h>
Preferences prefs;

// set audio volume
// #define I2S_DEFAULT_GAIN_LEVEL 0.7

// Color definitions
#ifndef DISABLE_COLOR_DEFINES
#define BLACK RGB565_BLACK
#define NAVY RGB565_NAVY
#define DARKGREEN RGB565_DARKGREEN
#define DARKCYAN RGB565_DARKCYAN
#define MAROON RGB565_MAROON
#define PURPLE RGB565_PURPLE
#define OLIVE RGB565_OLIVE
#define LIGHTGREY RGB565_LIGHTGREY
#define DARKGREY RGB565_DARKGREY
#define BLUE RGB565_BLUE
#define GREEN RGB565_GREEN
#define CYAN RGB565_CYAN
#define RED RGB565_RED
#define MAGENTA RGB565_MAGENTA
#define YELLOW RGB565_YELLOW
#define WHITE RGB565_WHITE
#define ORANGE RGB565_ORANGE
#define GREENYELLOW RGB565_GREENYELLOW
#define PALERED RGB565_PALERED
#define PINK 0xFE19
#endif

#define CANVAS

JC3248W535_Display display;
JC3248W535_Touch touch;
Arduino_Canvas *gfx; // Global pointer to the canvas

#define MAX_FILES 30
String aviFileList[MAX_FILES];

int fileCount = 0;
int selectedIndex = 0;

// Episode data structure
struct Episode {
    uint8_t number;
    const char* title;
    const char* filename;  // Added filename field
};

// Episode database with filenames
const Episode EPISODES[] = {
    {1, "Ship In A Bottle", "BP01.avi"},
    {2, "The Owls of Athens", "BP02.avi"},
    {3, "The Frog Princess", "BP03.avi"},
    {4, "The Ballet Shoe", "BP04.avi"},
    {5, "The Hamish", "BP05.avi"},
    {6, "The Wise Man", "BP06.avi"},
    {7, "The Elephant", "BP07.avi"},
    {8, "The Mouse Mill", "BP08.avi"},
    {9, "The Giant", "BP09.avi"},
    {10, "The Old Man's Beard", "BP10.avi"},
    {11, "The Fiddle", "BP11.avi"},
    {12, "Flying", "BP12.avi"},
    {13, "Uncle Feedle", "BP13.avi"}
};

const int NUM_EPISODES = sizeof(EPISODES) / sizeof(EPISODES[0]);

// Function to get episode title by number
const char* getEpisodeTitle(uint8_t episodeNumber) {
    if (episodeNumber < 1 || episodeNumber > NUM_EPISODES) {
        return "Invalid Episode";
    }
    return EPISODES[episodeNumber - 1].title;
}

// Array to store file names
char fileNames[MAX_FILES][13];  // 8.3 filename format: 8 chars + dot + 3 chars + null terminator
#include <string>

#define TITLE_REGION_Y 100
#define TITLE_REGION_H 40
#define TITLE_REGION_W (gfx->width())

#include <FFat.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SD_MMC.h>

size_t output_buf_size;
uint16_t *output_buf;

#include "AviFunc.h"
#include "esp32_audio.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("JC3248W535 Touch AVI Player");
  Serial.println("===========================");
  
  // Initialize display with rotation
  if (!display.begin()) {
    Serial.println("Display init failed!");
    while(1) delay(1000);
  }
  
  // Initialize touch
  touch.begin();
  
  // Set display rotation
  display.setRotation(ROTATION_90); // Landscape mode
  
  // Pass display rotation to touch driver for coordinate mapping
  display.setTouchRotation(&touch);

  gfx = display.getCanvas();

  // If display and SD shared same interface, init SPI first
  #ifdef SPI_SCK
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  #endif

  #ifdef GFX_EXTRA_PRE_INIT
    GFX_EXTRA_PRE_INIT();
  #endif

  #ifdef AUDIO_MUTE_PIN
    pinMode(AUDIO_MUTE_PIN, OUTPUT);
    digitalWrite(AUDIO_MUTE_PIN, HIGH);
  #endif

  prefs.begin("bagpuss", false);  // Open namespace "bagpuss"
  volumeLevel = prefs.getInt("volume", 5);  // Default to 5 if not found
  applyVolume();

  i2s_init();

  #if defined(SD_D1)
    #define FILESYSTEM SD_MMC
      SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */, SD_D1, SD_D2, SD_CS /* D3 */);
      if (!SD_MMC.begin(root, false /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
  #elif defined(SD_SCK)
    #define FILESYSTEM SD_MMC
      pinMode(SD_CS, OUTPUT);
      digitalWrite(SD_CS, HIGH);
      SD_MMC.setPins(SD_SCK, SD_MOSI /* CMD */, SD_MISO /* D0 */);
      if (!SD_MMC.begin(root, true /* mode1bit */, false /* format_if_mount_failed */, SDMMC_FREQ_HIGHSPEED))
  #elif defined(SD_CS)
    #define FILESYSTEM SD
      if (!SD.begin(SD_CS, SPI, 80000000, "/root"))
  #else
    #define FILESYSTEM FFat
      if (!FFat.begin(false, root))
      #endif
    {
      Serial.println("ERROR: File system mount failed!");
    }
  else
  {
    output_buf_size = gfx->width() * gfx->height() * 2;
    #ifdef CANVAS
        output_buf = gfx->getFramebuffer();
    #else
        output_buf = (uint16_t *)aligned_alloc(16, output_buf_size);
    #endif
    if (!output_buf)
      {
        Serial.println("output_buf aligned_alloc failed!");
      }

    avi_init();
      Serial.println("Completed avi_init");

    listAVIFiles(avi_folder);
      Serial.print("Found ");
      Serial.print(fileCount);
      Serial.println(" AVI files.");

    displaySelectedFile();
  }
}

void loop()
{
  TouchPoint point;

  int screenW = gfx->width();
  int screenH = gfx->height();
  int arrowSize = 40;
  int margin = 10;
  int playButtonSize = 60;
  int playX = (screenW - playButtonSize) / 2;
  int playY = (screenH + 60 - playButtonSize) / 2;
  int volBtnY = screenH - 30;
  int volBtnRadius = 30;



  if (playPressStart > 0 && !touch.isTouched()) {
    // Finger lifted - was it a short press?
    if (millis() - playPressStart < 2000) {
      if (autoMode) {
        // Short press on stop button - cancel auto mode
        autoMode = false;
        drawPlayButton();
        display.flush();
      } else {
        // Normal short press - play current episode
        String fullPath = String(root) + String(avi_folder) + "/" + aviFileList[selectedIndex];
        char aviFilename[128];
        fullPath.toCharArray(aviFilename, sizeof(aviFilename));
        playAviFile(aviFilename);
        displaySelectedFile();
      }
    }
    playPressStart = 0;
  }




  if (touch.read(point)) 
  {
    int tx = point.x;
    int ty = point.y;

    bool inPlayButton = (tx >= playX && tx <= playX + playButtonSize &&
                         ty >= playY && ty <= playY + playButtonSize);

    if (!inPlayButton) {
      playPressStart = 0;  // Reset only if touching somewhere else
    }

    // Check if touch is in the left arrow area (disabled in auto mode)
    if (!autoMode && tx < margin + arrowSize && ty > (screenH / 2 - arrowSize) && ty < (screenH / 2 + arrowSize))
    {
      // Left arrow touched: cycle to previous file.
      selectedIndex--;
      if (selectedIndex < 0)
        selectedIndex = fileCount - 1;
      updateTitle();
      while (touch.read(point)) { delay(50); }
      delay(300);
    }
    else if (!autoMode && tx > screenW - margin - arrowSize && ty > (screenH / 2 - arrowSize) && ty < (screenH / 2 + arrowSize))
    {
      // Right arrow touched: cycle to next file.
      selectedIndex++;
      if (selectedIndex >= fileCount)
        selectedIndex = 0;
      updateTitle();
      while (touch.read(point)) { delay(50); }
      delay(300);
    }
    // Check if touch is in the play/stop button area
    else if (inPlayButton)
    {
      if (playPressStart == 0) playPressStart = millis();
      if (millis() - playPressStart >= 2000) {
        // Long press threshold reached - activate auto mode and start playing
        autoMode = true;
        drawPlayButton();
        display.flush();
        playPressStart = 0;
        waitForTouchRelease();
        // Immediately start playing in auto mode
        String fullPath = String(root) + String(avi_folder) + "/" + aviFileList[selectedIndex];
        char aviFilename[128];
        fullPath.toCharArray(aviFilename, sizeof(aviFilename));
        playAviFile(aviFilename);
      }
    }

    // Volume buttons handled separately
    if (ty >= volBtnY - volBtnRadius && ty <= volBtnY + volBtnRadius) {
      if (tx >= 0 && tx <= 60) {
        // Minus button
        if (volumeLevel > 0) {
          volumeLevel--;
          applyVolume();
          volButtonPressed = true;
          drawVolumeControl();
          displaySelectedFile();
          display.flush();
          playVolumePreview();
          volButtonPressed = false;
        }
        waitForTouchRelease();
      }
      else if (tx >= screenW - 60 && tx <= screenW) {
        // Plus button
        if (volumeLevel < 7) {
          volumeLevel++;
          applyVolume();
          volButtonPressed = true;
          drawVolumeControl();
          displaySelectedFile();
          display.flush();
          playVolumePreview();
          volButtonPressed = false;
        }
        waitForTouchRelease();
      }
    }

  } // closes if (touch.read(point))

  // Auto mode: after episode ends, advance and auto-play after 3 second delay
  // (This is handled inside playAviFile when autoMode is true)

  delay(50);
}

// Continuously read until no touches are registered.
void waitForTouchRelease()
{
    TouchPoint point;
      while (touch.read(point))
  {
    delay(50);
  }
  // Extra debounce delay to ensure that the touch state is fully cleared.
  delay(300);
}


void drawVolumeControl() {
  int screenW = gfx->width();
  int screenH = gfx->height();

  int btnRadius = 18;
  int btnY = screenH - 30;  // Centre of buttons from bottom

  // Clear the entire volume control area first
  gfx->fillRect(0, btnY - btnRadius - 5, screenW, btnRadius * 2 + 10, PURPLE);

  // Draw minus button (left)
  int minusX = 30;
  gfx->drawCircle(minusX, btnY, btnRadius, WHITE);
  gfx->drawFastHLine(minusX - 8, btnY, 16, WHITE);

  // Draw plus button (right)
  int plusX = screenW - 30;
  gfx->drawCircle(plusX, btnY, btnRadius, WHITE);
  gfx->drawFastHLine(plusX - 8, btnY, 16, WHITE);
  gfx->drawFastVLine(plusX, btnY - 8, 16, WHITE);

  // Draw 7 volume bars in the centre
  int numBars = 7;
  int barW = 16;
  int barSpacing = 6;
  int maxBarH = 30;
  int minBarH = 8;
  int totalBarsW = numBars * barW + (numBars - 1) * barSpacing;
  int barsStartX = (screenW - totalBarsW) / 2;
  int barsBaseY = btnY + btnRadius;  // Bottom of bars aligned with button base

  for (int i = 0; i < numBars; i++) {
    int barH = minBarH + (maxBarH - minBarH) * i / (numBars - 1);
    int barX = barsStartX + i * (barW + barSpacing);
    int barY = barsBaseY - barH;

    if (i < volumeLevel) {
      gfx->fillRect(barX, barY, barW, barH, WHITE);  // Filled
    } else {
      gfx->drawRect(barX, barY, barW, barH, WHITE);  // Hollow
    }
  }
}

void applyVolume() {
  gainLevel = volumeLevel * 0.1f;
  prefs.putInt("volume", volumeLevel);  // Save to NVS
}


// Update the avi title on the screen
void updateTitle() {
  // Clear the entire title area
  gfx->fillRect(0, TITLE_REGION_Y, TITLE_REGION_W, TITLE_REGION_H, PURPLE);
  
  // Retrieve the new episode label
  String episodeLabel = getEpisodeTitle(selectedIndex + 1);
  
  // Get text dimensions for the episode label
  int16_t x1, y1;
  uint16_t textW, textH;
  gfx->setTextSize(1);
  gfx->setFont(u8g2_font_luRS24_tf);
  gfx->getTextBounds(episodeLabel.c_str(), 0, 0, &x1, &y1, &textW, &textH);

  // Center the text in the fixed title region:
  int titleX = (TITLE_REGION_W - textW) / 2 - x1;
  int titleY = TITLE_REGION_Y + (TITLE_REGION_H - textH) / 2 - y1;
  
  gfx->setCursor(titleX, titleY);
  gfx->setTextColor(WHITE);

  gfx->print(episodeLabel);
  display.flush();
}


void listAVIFiles(const char* dirPath) {

  File aviDir = FILESYSTEM.open(dirPath);
  if (!aviDir) {
    Serial.println("Failed to open directory");
    return;
  }
  
  while (true) {
    File entry = aviDir.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      if ((fileName.endsWith(".AVI") || fileName.endsWith(".avi")) && fileCount < MAX_FILES) 
      {
        aviFileList[fileCount++] = fileName;
        if (fileCount >= MAX_FILES)
          break;
      }
    }
    
    entry.close();
  }
  
  aviDir.close();
}


// Display the selected avi file
void displaySelectedFile()
{
    // Clear the screen
    gfx->fillScreen(PURPLE);
    drawBagpussTitle();

  int screenW = gfx->width();
  int screenH = gfx->height();
  int centerY = screenH / 2;
  int arrowSize = 40; // size of the arrow icon (adjust as needed)
  int margin = 10;    // margin from screen edge

  // --- Draw Left Arrow ---
  // The left arrow is drawn as a filled triangle at the left side.
  gfx->fillTriangle(margin, centerY,
                    margin + arrowSize, centerY - arrowSize / 2,
                    margin + arrowSize, centerY + arrowSize / 2,
                    RGB565_WHITE);

  // --- Draw Right Arrow ---
  // Draw the right arrow as a filled triangle at the right side.
  gfx->fillTriangle(screenW - margin, centerY,
                    screenW - margin - arrowSize, centerY - arrowSize / 2,
                    screenW - margin - arrowSize, centerY + arrowSize / 2,
                    RGB565_WHITE);

  // --- Draw the Title ---
  // Clear the entire title area
  gfx->fillRect(0, TITLE_REGION_Y, TITLE_REGION_W, TITLE_REGION_H, PURPLE);
  
  // Retrieve the new episode label
  String episodeLabel = getEpisodeTitle(selectedIndex + 1);
  
  // Get text dimensions for the episode label
  int16_t x1, y1;
  uint16_t textW, textH;
  gfx->setTextSize(1);
  gfx->setFont(u8g2_font_luRS24_tf);
  gfx->getTextBounds(episodeLabel.c_str(), 0, 0, &x1, &y1, &textW, &textH);

  // Center the text in the fixed title region:
  int titleX = (TITLE_REGION_W - textW) / 2 - x1;
  int titleY = TITLE_REGION_Y + (TITLE_REGION_H - textH) / 2 - y1;
  
  gfx->setCursor(titleX, titleY);
  gfx->setTextColor(WHITE);

  gfx->print(episodeLabel);

  // --- Draw the Play/Stop Button ---
  drawPlayButton();

  applyVolume();
  drawVolumeControl();
  display.flush();
}


void drawBagpussTitle() {
  const char* title = "Bagpuss Box";
  int len = strlen(title);
  
  // Set text size for title
  gfx->setTextSize(2);
  gfx->setFont(u8g2_font_mystery_quest_32_tr);

  // Measure total width of the title to centre it
  int16_t x1, y1;
  uint16_t textW, textH;
  gfx->getTextBounds(title, 0, 0, &x1, &y1, &textW, &textH);
  
  int startX = (gfx->width() - textW) / 2 - x1;
  int startY = 10 - y1;  // 10px from top of screen
  
  // Draw each letter one at a time
  int cursorX = startX;
  for (int i = 0; i < len; i++) {
    // Alternate pink and white
    uint16_t colour = (i % 2 == 0) ? PINK  : WHITE;
    
    gfx->setTextColor(colour);
    gfx->setCursor(cursorX, startY);
    
    // Print single character
    char letter[2] = { title[i], '\0' };
    if (title[i] == ' ') {
      cursorX += 12;  // Manual space width
      continue;
    }
    gfx->print(letter);
    display.flush();
    
    // Measure this character's width to advance cursor
    uint16_t charW, charH;
    gfx->getTextBounds(letter, 0, 0, &x1, &y1, &charW, &charH);
    cursorX += charW;
    
    delay(100 * !volButtonPressed);  // Adjust for desired speed
  }
}

// Draw play or stop button depending on autoMode
void drawPlayButton() {
  int screenW = gfx->width();
  int screenH = gfx->height();
  int playButtonSize = 60;
  int playX = (screenW - playButtonSize) / 2;
  int playY = (screenH + 60 - playButtonSize) / 2;
  int cx = playX + playButtonSize / 2;
  int cy = playY + playButtonSize / 2;

  if (autoMode) {
    // Draw stop button: red circle with white square
    gfx->fillCircle(cx, cy, playButtonSize / 2, RED);
    int squareSize = playButtonSize / 3;
    gfx->fillRect(cx - squareSize / 2, cy - squareSize / 2, squareSize, squareSize, WHITE);
  } else {
    // Draw normal play button: dark green circle with white triangle
    gfx->fillCircle(cx, cy, playButtonSize / 2, RGB565_DARKGREEN);
    int triX = cx - playButtonSize / 4 + 2;
    gfx->fillTriangle(triX, cy - playButtonSize / 4,
                      triX, cy + playButtonSize / 4,
                      triX + playButtonSize / 2, cy,
                      WHITE);
  }
}

// Play first 3 seconds of current episode as volume preview
void playVolumePreview() {
  String fullPath = String(root) + String(avi_folder) + "/" + aviFileList[selectedIndex];
  char aviFilename[128];
  fullPath.toCharArray(aviFilename, sizeof(aviFilename));

  if (avi_open(aviFilename)) {
    i2s_set_sample_rate(avi_aRate);
    avi_feed_audio();
    BaseType_t ret_val = mp3_player_task_start();
    if (ret_val != pdPASS) {
      Serial.printf("mp3_player_task_start failed: %d\n", ret_val);
    }
    unsigned long previewStart = millis();
    while (avi_curr_frame < avi_total_frames && millis() - previewStart < 3000) {
      avi_feed_audio();
      if (avi_decode()) {
        // Decode audio only, don't draw video frames
      }
    }
    avi_close();
  }
}

// Play a single avi file stored on the SD card
void playAviFile(char *avifile)
{
  if (avi_open(avifile))
  {
    Serial.printf("AVI start %s\n", avifile);
    gfx->fillScreen(BLACK);

    i2s_set_sample_rate(avi_aRate);

    avi_feed_audio();

    Serial.println("Start play audio task");
    BaseType_t ret_val = mp3_player_task_start();
    if (ret_val != pdPASS)
    {
      Serial.printf("mp3_player_task_start failed: %d\n", ret_val);
    }

    avi_start_ms = millis();

    Serial.println("Start play loop");
    while (avi_curr_frame < avi_total_frames)
    {
      avi_feed_audio();
      if (avi_decode())
      {
        avi_draw(0, 0);
      }
    }

    avi_close();
    Serial.println("AVI end");

    // Auto mode: advance to next episode and auto-play after 3 second delay
    if (autoMode) {
      selectedIndex++;
      if (selectedIndex >= fileCount) {
        // Reached last episode - stop auto mode
        autoMode = false;
        displaySelectedFile();
        return;
      }
      // Show selection screen with next episode for 3 seconds
      displaySelectedFile();
      delay(3000);
      // Check if auto mode was cancelled during the delay
      if (autoMode) {
        String fullPath = String(root) + String(avi_folder) + "/" + aviFileList[selectedIndex];
        char nextFile[128];
        fullPath.toCharArray(nextFile, sizeof(nextFile));
        playAviFile(nextFile);
      }
    }
  }
  else
  {
    Serial.println(AVI_strerror());
  }
}
