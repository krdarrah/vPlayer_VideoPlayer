#include <WiFi.h>
#include <SD_MMC.h>  // Use SD_MMC library for SDMMC interface
#include "esp_ota_ops.h"
#include "Arduino_GFX_Library.h"// 1.4.7
#include "driver/ledc.h"
#include <CST816S.h>// 1.1.1
#include "icelandFont.h"
#define TFT_BRIGHTNESS 255
#define MJPEG_BUFFER_SIZE (280 * 240 * 3 + 1024)


// Pin definitions
#define TFT_BL 9
#define SCK 12
#define MOSI 11
#define MISO 11  // ??just some random pin

int sdMMC_CMD = 35;  // Command pin
int sdMMC_CLK = 36;  // Clock pin
int sdMMC_D0 = 37;   // Data 0 pin
int sdMMC_D1 = 38;   // Data 1 pin (for 4-bit mode)
int sdMMC_D2 = 33;   // Data 2 pin (for 4-bit mode)
int sdMMC_D3 = 34;   // Data 3 pin (for 4-bit mode)

// Define firmware file path
const char* firmwareFilePath = "/firmware.bin";

// ST7789 Display
Arduino_HWSPI* bus = new Arduino_HWSPI(13 /* DC */, 10 /* CS */, SCK, MOSI, MISO);
Arduino_GFX* gfx = new Arduino_ST7789(bus, 14 /* RST */, 0 /* rotation */, true /* IPS */, 240 /* width */, 280 /* height */, 0 /* col offset 1 */, 276 /* row offset 1 */);

// Include and initialize touchpad
CST816S touch(16, 15, 17, 18);  // sda, scl, rst, irq

#include "MjpegClass.h"
#include "tjpgdClass.h"
static MjpegClass mjpeg;

// Centered position of the buttons within the 280px window
int buttonWidth = 240;                       // Width of each button to ensure equal space on either side
int buttonStartX = (280 - buttonWidth) / 2;  // Horizontal centering calculation
int buttonStartY = 80;                       // Starting Y position to center the buttons vertically
int buttonHeight = 50;
int buttonSpacing = 20;  // Space between buttons


// Function to check and update firmware from SD card
void checkAndUpdateFirmware() {
  if (!SD_MMC.begin("/sdcard", false)) {
    Serial.println(F("ERROR: SD card mount failed!"));
    return;
  }

  // Check for firmware file
  if (SD_MMC.exists(firmwareFilePath)) {
    Serial.println(F("Firmware update file found! Starting update..."));

    File updateFile = SD_MMC.open(firmwareFilePath);
    if (!updateFile) {
      Serial.println(F("ERROR: Failed to open firmware file"));
      return;
    }

    // Start OTA
    esp_ota_handle_t otaHandle;
    const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(NULL);
    if (esp_ota_begin(updatePartition, OTA_SIZE_UNKNOWN, &otaHandle) != ESP_OK) {
      Serial.println(F("ERROR: OTA begin failed"));
      updateFile.close();
      return;
    }

    // Write firmware file to flash
    while (updateFile.available()) {
      uint8_t buffer[1024];
      int bytesRead = updateFile.read(buffer, sizeof(buffer));
      if (esp_ota_write(otaHandle, buffer, bytesRead) != ESP_OK) {
        Serial.println(F("ERROR: OTA write failed"));
        esp_ota_end(otaHandle);
        updateFile.close();
        return;
      }
    }

    // Finalize OTA update
    if (esp_ota_end(otaHandle) == ESP_OK) {
      if (esp_ota_set_boot_partition(updatePartition) == ESP_OK) {
        Serial.println(F("Firmware update completed successfully!"));
        updateFile.close();
        SD_MMC.remove(firmwareFilePath);  // Delete firmware file
        esp_restart();                    // Restart to apply the update
      } else {
        Serial.println(F("ERROR: OTA set boot partition failed"));
      }
    } else {
      Serial.println(F("ERROR: OTA end failed"));
    }
  } else {
    Serial.println(F("No firmware update file found."));
  }
}
// Function to display a modern GUI for shuffle or in-order play selection with animation
void displayPlayModeSelection() {
  gfx->setRotation(1);  // Rotate the display by 90 degrees clockwise
  gfx->fillScreen(BLACK);

  // Draw header text with a sleek style
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setFont(&Iceland_Regular20pt7b);
  gfx->setCursor(55, 30);  // Adjusted to position the text better in the header
  gfx->println("Play Mode");


  // Draw "Shuffle" button with rounded corners using a modern color scheme
  gfx->fillRoundRect(buttonStartX, buttonStartY, buttonWidth, buttonHeight, 10, NAVY);  // Button background
  gfx->drawRoundRect(buttonStartX, buttonStartY, buttonWidth, buttonHeight, 10, BLUE);  // Slightly lighter border
  gfx->setTextColor(WHITE, NAVY);
  gfx->setCursor(buttonStartX + (buttonWidth / 2) - 56, buttonStartY + 35);  // Centered text within the button
  gfx->println("Shuffle");

  // Draw "In-Order" button with rounded corners using a different color scheme
  gfx->fillRoundRect(buttonStartX, buttonStartY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight, 10, DARKGREEN);  // Button background
  gfx->drawRoundRect(buttonStartX, buttonStartY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight, 10, GREEN);      // Slightly lighter border
  gfx->setTextColor(WHITE, DARKGREEN);
  gfx->setCursor(buttonStartX + (buttonWidth / 2) - 62, buttonStartY + buttonHeight + buttonSpacing + 35);  // Centered text within the button
  gfx->println("In-Order");

  // Call the improved snowflake animation while waiting for input
  improvedSnowflakeAnimation();
}


// Improved function for the snowflake animation with targeted clearing
void improvedSnowflakeAnimation() {
  struct Snowflake {
    int x, y;
    int speed;
  };

  const int maxSnowflakes = 20;  // Number of snowflakes
  Snowflake snowflakes[maxSnowflakes];
  uint16_t snowflakeColor = LIGHTGREY;

  // Initialize snowflakes at random positions with random speeds
  for (int i = 0; i < maxSnowflakes; i++) {
    snowflakes[i].x = random(gfx->width());
    snowflakes[i].y = random(gfx->height());
    snowflakes[i].speed = random(1, 3);
  }

  // Main loop to animate the snowflakes without affecting GUI elements
  while (!touch.available()) {  // Continue animation until touch detected
    for (int i = 0; i < maxSnowflakes; i++) {
      // Erase the old snowflake position by redrawing background color only in the allowed regions
      if (isSnowflakeInAllowedRegion(snowflakes[i].x, snowflakes[i].y)) {
        gfx->drawPixel(snowflakes[i].x, snowflakes[i].y, BLACK);
      }

      // Update snowflake position
      snowflakes[i].y += snowflakes[i].speed;
      if (snowflakes[i].y >= gfx->height()) {
        snowflakes[i].x = random(gfx->width());
        snowflakes[i].y = 0;
      }

      // Draw new snowflake only if it is outside the exclusion zones
      if (isSnowflakeInAllowedRegion(snowflakes[i].x, snowflakes[i].y)) {
        gfx->drawPixel(snowflakes[i].x, snowflakes[i].y, snowflakeColor);
      }
    }
    delay(50);  // Small delay to control animation speed
  }
}

// Function to check if a snowflake is in the allowed region for drawing
bool isSnowflakeInAllowedRegion(int x, int y) {
  // Define exclusion zones for the title and button areas, adding a 1-pixel buffer
  const int exclusionTop = 10;      // Top exclusion for the "Play Mode" text
  const int exclusionBottom = 270;  // Bottom of the screen, ensuring snowflakes don't interfere with buttons
  const int exclusionLeft = 15;
  const int exclusionRight = 265;
  const int buttonTop1 = 79;      // Top of the first button with 1-pixel buffer
  const int buttonBottom1 = 131;  // Bottom of the first button with 1-pixel buffer
  const int buttonTop2 = 149;     // Top of the second button with 1-pixel buffer
  const int buttonBottom2 = 201;  // Bottom of the second button with 1-pixel buffer

  // Allow snowflakes in the areas between the title, buttons, and other regions
  return !((y > exclusionTop && y < 55 && x > exclusionLeft && x < exclusionRight) ||           // Exclude the title area
           (y > buttonTop1 && y < buttonBottom1 && x > exclusionLeft && x < exclusionRight) ||  // Exclude Shuffle Play button area
           (y > buttonTop2 && y < buttonBottom2 && x > exclusionLeft && x < exclusionRight));   // Exclude In-Order Play button area
}

// Function to get user input for play mode selection with 90-degree rotation
bool getPlayModeSelection() {
  while (true) {
    if (touch.available()) {
      uint16_t x = touch.data.y;  // Swap x and y for the rotated orientation
      uint16_t y = touch.data.x;  // Swap x and y for the rotated orientation

      Serial.print("Touch detected at: x=");
      Serial.print(x);
      Serial.print(", y=");
      Serial.println(y);

      // Button positions based on swipe data
      int shuffleButtonYStart = 100;  // Upper Y bound of the Shuffle button
      int shuffleButtonYEnd = 150;    // Lower Y bound of the Shuffle button
      int shuffleButtonXStart = 0;    // X start of the Shuffle button
      int shuffleButtonXEnd = 255;    // X end of the Shuffle button

      int inOrderButtonYStart = 30;  // Upper Y bound of the In-Order button
      int inOrderButtonYEnd = 80;    // Lower Y bound of the In-Order button
      int inOrderButtonXStart = 0;   // X start of the In-Order button
      int inOrderButtonXEnd = 255;   // X end of the In-Order button

      // Check if the touch is within the "Shuffle Play" button
      if (y >= shuffleButtonYStart && y <= shuffleButtonYEnd && x >= shuffleButtonXStart && x <= shuffleButtonXEnd) {
        Serial.println(F("Shuffle Play selected."));
        //gfx->fillRoundRect(buttonStartX, buttonStartY, buttonWidth, buttonHeight, 10, BLUE);  // Button background
        gfx->drawRoundRect(buttonStartX, buttonStartY, buttonWidth, buttonHeight, 10, NAVY);  // Slightly lighter border
        delay(500);
        return true;  // Shuffle play
      }
      // Check if the touch is within the "In-Order Play" button
      else if (y >= inOrderButtonYStart && y <= inOrderButtonYEnd && x >= inOrderButtonXStart && x <= inOrderButtonXEnd) {
        Serial.println(F("In-Order Play selected."));
        //gfx->fillRoundRect(buttonStartX, buttonStartY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight, 10, GREEN);      // Button background
        gfx->drawRoundRect(buttonStartX, buttonStartY + buttonHeight + buttonSpacing, buttonWidth, buttonHeight, 10, DARKGREEN);  // Slightly lighter border
        delay(500);
        return false;  // In-order play
      }
    }
    delay(100);
  }
}

void shuffleFiles(char* arr[], int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = random(i + 1);
    char* temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
  }
}
void playAllVideosInOrder(bool shuffle) {

  // Initialize SD card
  if (!SD_MMC.begin("/sdcard", false)) {
    Serial.println(F("ERROR: SD card mount failed! Check connections and formatting."));
    gfx->println(F("ERROR: SD card mount failed!"));
    return;
  } else {
    Serial.println(F("SD card initialized successfully!"));
  }

  Serial.println(F("Looking for files"));

  // Array to hold MJPEG file names
  char* mjpegFiles[100];
  int fileCount = 0;

  // Open SD card root directory and find MJPEG files
  File root = SD_MMC.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }

    String filename = entry.name();
    Serial.print(F("Checking file: '"));
    Serial.print(filename);
    Serial.println(F("'"));
    if (filename.startsWith(".")) continue;  // Ignore hidden files

    if (!entry.isDirectory()) {
      if (filename.endsWith(".mjpeg")) {
        Serial.print(F("Found MJPEG file: "));
        Serial.println(filename);
        mjpegFiles[fileCount] = strdup(filename.c_str());  // Store file name
        fileCount++;
      }
    }
    entry.close();
  }

  // If no MJPEG files are found, stop the function
  if (fileCount == 0) {
    Serial.println(F("No MJPEG files found."));
    return;
  }

  // Shuffle the MJPEG files to randomize playback order if selected
  if (shuffle) {
    shuffleFiles(mjpegFiles, fileCount);
  }

  // Allocate buffer for MJPEG playback
  uint8_t* mjpeg_buf = (uint8_t*)heap_caps_malloc(MJPEG_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  if (!mjpeg_buf) {
    Serial.println(F("ERROR: Failed to allocate mjpeg_buf in PSRAM."));
    return;
  }

  int currentFileIndex = 0;
  int consecutiveErrors = 0;  // Counter for consecutive frame failures
  bool videoFailed = false;
  int frameCount = 0;
  int fps;
  unsigned long lastTime;
  // Main loop to play videos in sequence
  while (true) {
    Serial.print(F("Playing video: "));
    Serial.println(mjpegFiles[currentFileIndex]);

    String fullPath = String("/") + mjpegFiles[currentFileIndex];
    File videoFile = SD_MMC.open(fullPath);

    if (videoFile) {
      videoFile.seek(0);  // Reset file to the beginning

      // Reset MJPEG decoder and setup the video file
      mjpeg.reset();
      mjpeg.setup(videoFile, mjpeg_buf, (Arduino_TFT*)gfx, true);

      unsigned long frameStart;

      // Read and play each frame
      while (mjpeg.readMjpegBuf()) {
        unsigned long frameStart = millis();

        // Try to render the frame
        if (!mjpeg.drawJpg()) {
          Serial.println(F("video skipped due to error"));
          break;
        }

        // frameCount++;

        // // Update FPS every second
        // if (millis() - lastTime >= 1000) {
        //   fps = frameCount;
        //   frameCount = 0;
        //   lastTime = millis();

        //   Serial.println(fps);
        // }

        //Frame rate control: Wait for the frame to finish
        while (millis() - frameStart < 55) {}

        //Handle touch event to skip to the next video
        if (touch.available() && touch.data.event == 0x01) {
          Serial.println(F("Single click detected, advancing to the next video."));
          break;  // Exit the loop to move to the next video
        }
      }

      videoFile.close();  // Close the video file

      // Skip the current video if it encountered too many errors
      if (videoFailed) {
        Serial.println(F("Skipping video due to repeated errors."));
        mjpeg.reset();        // Reset the decoder to clear any issues
        videoFailed = false;  // Reset flag for the next video
      }
    } else {
      Serial.print(F("ERROR: Failed to open file: "));
      Serial.println(fullPath);
    }

    // Move to the next video in the list
    currentFileIndex++;
    if (currentFileIndex >= fileCount) {
      currentFileIndex = 0;  // Loop back to the first video
    }
  }

  // Cleanup: Free allocated memory
  free(mjpeg_buf);
  for (int i = 0; i < fileCount; i++) {
    free(mjpegFiles[i]);
  }
}


void setup() {
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);
  touch.begin();

  // Set custom SDMMC pins
  if (!SD_MMC.setPins(sdMMC_CLK, sdMMC_CMD, sdMMC_D0, sdMMC_D1, sdMMC_D2, sdMMC_D3)) {
    Serial.println(F("ERROR: Failed to set custom SDMMC pins!"));
    return;
  }

#ifdef TFT_BL
  ledcSetup(1, 12000, 8);        // 12 kHz PWM, 8-bit resolution
  ledcAttachPin(TFT_BL, 1);      // assign TFT_BL pin to channel 1
  ledcWrite(1, TFT_BRIGHTNESS);  // brightness 0 - 255
#endif

  delay(1000);
  gfx->begin(60000000);

  gfx->fillScreen(RED);
  delay(100);
  gfx->fillScreen(GREEN);
  delay(100);
  gfx->fillScreen(BLUE);
  delay(100);
  checkAndUpdateFirmware();  // Check for firmware update at boot

  // Display play mode selection GUI
  displayPlayModeSelection();
  bool shuffle = getPlayModeSelection();
  gfx->setRotation(0);  // Reset rotation to original orientation
  playAllVideosInOrder(shuffle);
}

void loop() {
  // Empty loop since playAllVideosInOrder() runs indefinitely
}