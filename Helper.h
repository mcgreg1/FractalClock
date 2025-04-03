#ifndef HELPER_H
#define HELPER_H

#include <Arduino_GFX_Library.h> // Needed for GFXfont type, tm struct
#include <time.h> // For tm struct

// --- Function Declarations ---

// WiFi & NTP
void connectToWiFi();
void syncTimeNTP();
void resyncTimeNTP();

// Display Drawing
void displayClock(const struct tm* currentTime);
void displayStaticElements(const struct tm* currentTime);
void displaySetTimeScreen(const struct tm* currentTimeToDisplay);
void displayTouchCoords(); // New function for touch coordinates
void centerText(const char *text, int y, uint16_t color, const GFXfont *font, uint8_t size);

// Touch Handling
void handleTimeSettingTouch();

// Time & Math
int calculateFutureTimeNum(int currentHour, int currentMin, int currentSec, int secondsToAdd);
void primeFactorsToString(int n, char* buffer, size_t bufferSize); // New synchronous factor function

// Initialization helper (called from setup)
void initializeColors();

#endif // HELPER_H