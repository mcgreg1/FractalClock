#ifndef CUSTOMDEF_H
#define CUSTOMDEF_H

// --- Library Includes ---
#include <WiFi.h>                  // For WiFi
#include <WiFiManager.h>
#include <time.h>                  // For NTP time and time functions
#include <Wire.h>                  // For I2C communication
#include <TAMC_GT911.h>            // GT911 Touch library
#include <Arduino_GFX_Library.h>   // Graphics library base

// --- Font Includes ---
// Make sure paths are correct for your system

#include "C:\Users\mcgre\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSansBold18pt7b.h"
#include "C:\Users\mcgre\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSans18pt7b.h"
#include "C:\Users\mcgre\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSans12pt7b.h"

/*
#include "C:\Users\mcgreg\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSansBold18pt7b.h"
#include "C:\Users\mcgreg\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSans18pt7b.h"
#include "C:\Users\mcgreg\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSans12pt7b.h"
*/
// --- Display Configuration ---
#define GFX_BL 38
extern Arduino_DataBus *bus; // Declare as extern, define in .ino
extern Arduino_ESP32RGBPanel *rgbpanel; // Declare as extern
extern Arduino_RGB_Display *gfx;      // Declare as extern
//extern const uint8_t st7701_type5_init_operations[]; // Declare if used, define elsewhere if needed
//extern const int st7701_type5_init_operations_len;  // Declare if used

// --- WiFi Credentials ---
extern const char *ssid;     // DECLARATION ONLY
extern const char *password; // DECLARATION ONLY

// WiFiManager Object
extern WiFiManager wifiManager; // <<< ADDED

// --- NTP Settings ---
extern const char *ntpServer; // DECLARATION ONLY
extern const long gmtOffset_sec; // DECLARATION ONLY
extern const int daylightOffset_sec; // DECLARATION ONLY
extern const long ntpSyncInterval; // DECLARATION ONLY

// German weekdays and months
extern const char *Wochentage[];     // DECLARATION ONLY
extern const char *Monate[];         // DECLARATION ONLY

extern const long interval;           // DECLARATION ONLY

// --- Touch Screen Pin Configuration ---
#define TOUCH_GT911_SDA 19 // I2C Data Pin
#define TOUCH_GT911_SCL 45 // I2C Clock Pin
#define TOUCH_GT911_INT -1 // Interrupt Pin (-1 if not used/connected)
#define TOUCH_GT911_RST -1 // Reset Pin (-1 if not used/connected)

// --- Clock States ---
enum ClockState {
    STATE_BOOTING,
    STATE_WAITING_FOR_WIFI,
    STATE_WAITING_FOR_NTP,
    STATE_SETTING_TIME,     // Manual time set mode
    STATE_RUNNING_NTP,      // Running with NTP-synced time
    STATE_RUNNING_MANUAL    // Running with manually set time
};

// --- Touch Zones for Time Setting ---
#define TIME_SET_Y_OFFSET 20
#define HOUR_TOUCH_X      (w * 0.1)
#define HOUR_TOUCH_W      (w * 0.3)
#define MINUTE_TOUCH_X    (w * 0.6)
#define MINUTE_TOUCH_W    (w * 0.3)
#define TIME_SET_TOUCH_H  60

#define OK_BUTTON_W       100
#define OK_BUTTON_H       50
#define OK_BUTTON_X       ((w - OK_BUTTON_W) / 2)
// OK_BUTTON_Y is calculated dynamically

#define NO_WIFI_BUTTON_W  180 // Width for "Ohne Wifi" button
#define NO_WIFI_BUTTON_H  50  // Height
#define NO_WIFI_BUTTON_X  ((w - NO_WIFI_BUTTON_W) / 2) // Center horizontally
#define NO_WIFI_BUTTON_Y_MARGIN 40 // Space below last text line
extern uint16_t COLOR_BUTTON_GREY; // Button background color

// --- Factor String Buffer Size ---
#define MAX_FACTOR_STR_LEN 512 // Max length for factor string storage

// --- Global Variables ---
extern int32_t w;                // Screen width (defined in .ino after GFX init)
extern int32_t h;                // Screen height (defined in .ino after GFX init)
extern struct tm timeinfo;             // Structure to hold time information
extern unsigned long previousMillis;   // For 1-second tick
extern const long interval;           // Update interval (1 second)
extern bool timeSynchronized;         // Flag if time is valid (NTP or Manual)
extern unsigned long lastNtpSyncMillis; // Timer for NTP re-sync
extern ClockState currentClockState;     // Current state of the clock

// German weekdays and months
extern const char *Wochentage[];
extern const char *Monate[];

// --- Time Setting Screen Layout Globals (GFX Coordinates, 0,0 Top-Left) ---
extern int set_time_y_top;         // Calculated Top Y edge of HH:MM:SS text
extern int set_time_touch_y_top;   // Calculated Top Y edge of the touch boxes below time
extern int set_time_hour_x_left;   // Calculated Left X edge of Hour touch box
extern int set_time_minute_x_left; // Calculated Left X edge of Minute touch box
extern int set_time_box_width;     // Width for BOTH Hour and Minute touch boxes
extern int set_time_box_height;    // Height for BOTH Hour and Minute touch boxes
extern int set_time_ok_y_top;      // Calculated Top Y edge of OK button box
extern int set_time_ok_x_left;     // Calculated Left X edge of OK button box (uses OK_BUTTON_X)


// --- Color Definitions ---
#define BLACK 0x0000
#define WHITE 0xFFFF
#define YELLOW 0xFFE0
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
extern uint16_t RGB565_LIGHT_GREY; // Define in Helper.cpp using gfx
extern uint16_t COLOR_OK_BUTTON_GREEN;      // Define in Helper.cpp using gfx
// Add a color for the debug boxes
#define RGB565_DEBUG_CYAN gfx->color565(0, 255, 255) // Define in Helper.cpp
extern uint16_t COLOR_DEBUG_CYAN; // Declare extern

// --- Global Touch Object Pointer ---
extern TAMC_GT911 *ts_ptr; // Define in .ino

// Touch State Variables
extern bool touchRegisteredThisPress;
extern bool needsStaticRedraw;
extern bool firstDisplayDone;

// Touch Coordinate Display Variables
extern unsigned long lastTouchMillis;
extern const unsigned long touchDisplayTimeout;
extern bool touchCoordsVisible;
extern int lastDisplayedTouchX;
extern int lastDisplayedTouchY;
extern char previousTouchCoordsStr[20]; // For clearing previous coords text
extern uint16_t prev_touch_coords_x;    // Position info for clearing
extern uint16_t prev_touch_coords_y;
extern uint16_t prev_touch_coords_w;
extern uint16_t prev_touch_coords_h;

// Font pointers (useful if fonts are large)
extern const GFXfont *font_freesansbold18;
extern const GFXfont *font_freesans18;
extern const GFXfont *font_freesans12;

#endif // CUSTOMDEF_H