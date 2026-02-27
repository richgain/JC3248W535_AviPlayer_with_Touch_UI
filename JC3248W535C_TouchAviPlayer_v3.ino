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

// Choose your rotation mode:
// ROTATION_90  = Landscape (480x320)

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

  if (touch.read(point)) 
  {
    // int tx = touchController.points[0].x;
    // int ty = touchController.points[0].y;
    int tx = point.x;
    int ty = point.y;
    int screenW = gfx->width();
    int screenH = gfx->height();
    int arrowSize = 40;
    int margin = 10;
    int playButtonSize = 50;
    int playX = (screenW - playButtonSize) / 2;
    int playY = screenH - playButtonSize - 20;

    // Check if touch is in the left arrow area.
    if (tx < margin + arrowSize && ty > (screenH / 2 - arrowSize) && ty < (screenH / 2 + arrowSize))
    {
      // Left arrow touched: cycle to previous file.
      selectedIndex--;
      Serial.println(selectedIndex);
      if (selectedIndex < 0)
        selectedIndex = fileCount - 1;
      updateTitle();
      while (touch.read(point))
      {
        delay(50);
      }
      delay(300);
    }
    else if (tx > screenW - margin - arrowSize && ty > (screenH / 2 - arrowSize) && ty < (screenH / 2 + arrowSize))
    {
      // Right arrow touched: cycle to next file.
      selectedIndex++;
      Serial.println(selectedIndex);
      if (selectedIndex >= fileCount)
        selectedIndex = 0;
      updateTitle();
      while (touch.read(point))
      {
        delay(50);
      }
      delay(300);
    }
    // Check if touch is in the play button area.
    else if (tx >= playX && tx <= playX + playButtonSize &&
             ty >= playY && ty <= playY + playButtonSize)
    {
      // Build the full path and play the selected file.
      String fullPath = String(root) + String(avi_folder) + "/" + aviFileList[selectedIndex];
      char aviFilename[128];
      fullPath.toCharArray(aviFilename, sizeof(aviFilename));
      Serial.println(aviFilename); 
      Serial.println(selectedIndex);
      Serial.println(fullPath);

      playAviFile(aviFilename);

      // Wait until the user fully releases the touch before refreshing the UI.
      waitForTouchRelease();

      // After playback, redisplay the selection screen.
      displaySelectedFile();
      while (touch.read(point))
      {
        delay(50);
      }
      delay(300);
    }
  }
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

// Update the avi title on the screen
void updateTitle() {
  // Clear the entire title area
  gfx->fillRect(0, TITLE_REGION_Y, TITLE_REGION_W, TITLE_REGION_H, PURPLE);
  
  // Retrieve the new episode label
  String episodeLabel = getEpisodeTitle(selectedIndex + 1);
  
  // Get text dimensions for the episode label
  int16_t x1, y1;
  uint16_t textW, textH;
  gfx->setTextSize(3);
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
  gfx->setTextSize(3);
  gfx->getTextBounds(episodeLabel.c_str(), 0, 0, &x1, &y1, &textW, &textH);

  // Center the text in the fixed title region:
  int titleX = (TITLE_REGION_W - textW) / 2 - x1;
  int titleY = TITLE_REGION_Y + (TITLE_REGION_H - textH) / 2 - y1;
  
  gfx->setCursor(titleX, titleY);
  gfx->setTextColor(WHITE);

  gfx->print(episodeLabel);

  // --- Draw the Play Button ---
  // Define the play button size and location.
  int playButtonSize = 50;
  int playX = (screenW - playButtonSize) / 2;
  int playY = screenH - playButtonSize - 20; // 20 pixels from bottom
  // Draw a filled circle for the button background.
  gfx->fillCircle(playX + playButtonSize / 2, playY + playButtonSize / 2, playButtonSize / 2, RGB565_DARKGREEN);
  // Draw a play–icon (triangle) inside the circle.
  int triX = playX + playButtonSize / 2 - playButtonSize / 4 + 2;
  int triY = playY + playButtonSize / 2;
  gfx->fillTriangle(triX, triY - playButtonSize / 4,
                    triX, triY + playButtonSize / 4,
                    triX + playButtonSize / 2, triY,
                    RGB565_WHITE);

  display.flush();
}


void drawBagpussTitle() {
  const char* title = "Bagpuss Box";
  int len = strlen(title);
  
  // Set text size for title
  gfx->setTextSize(4);
  
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
    gfx->print(letter);
    display.flush();
    
    // Measure this character's width to advance cursor
    uint16_t charW, charH;
    gfx->getTextBounds(letter, 0, 0, &x1, &y1, &charW, &charH);
    cursorX += charW;
    
    delay(300);  // Adjust for desired speed
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

  }
  else
  {
    Serial.println(AVI_strerror());
  }
}
