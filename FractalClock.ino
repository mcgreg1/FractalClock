// FractalClock Main Sketch - Simplified (No Task/Queue/Cache) + Touch

#include "CustomDef.h" // Includes, Defines, Globals
#include "Helper.h"    // Function Declarations

// --- Global Variable Definitions ---
// Screen dimensions (initialized after GFX init)
int32_t w = 0;
int32_t h = 0;
// Timekeeping
struct tm timeinfo;
unsigned long previousMillis = 0;

bool timeSynchronized = false;
unsigned long lastNtpSyncMillis = 0;
ClockState currentClockState = STATE_BOOTING;
// Touch
TAMC_GT911 *ts_ptr = nullptr;
bool touchRegisteredThisPress = false;
bool needsStaticRedraw = false;
// Touch Coordinate Display
unsigned long lastTouchMillis = 0;
const unsigned long touchDisplayTimeout = 3000; // ms
bool touchCoordsVisible = false;
int lastDisplayedTouchX = 0;
int lastDisplayedTouchY = 0;
char previousTouchCoordsStr[20] = "";
uint16_t prev_touch_coords_x = 0;
uint16_t prev_touch_coords_y = 0;
uint16_t prev_touch_coords_w = 0;
uint16_t prev_touch_coords_h = 0;


// --- Display Driver Instantiation (Matching CustomDef.h externs) ---
// This needs to match EXACTLY what was in your working minimal GFX test
// Assuming st7701_type5_init_operations is defined elsewhere or handled internally
extern const uint8_t st7701_type5_init_operations[];
const int st7701_type5_init_operations_len = sizeof(st7701_type5_init_operations); // Define length if using sizeof

Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, 39 /* CS */,
    48 /* SCK */, 47 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 5 /* R1 */, 6 /* R2 */, 7 /* R3 */, 15 /* R4 */,
    8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
    11 /* B0 */, 12 /* B1 */, 13 /* B2 */, 14 /* B3 */, 0 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, 12000000 /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 0 /* bounce_buffer_size_px */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type5_init_operations, st7701_type5_init_operations_len); // Use defined length

// --- Setup ---
void setup() {
    currentClockState = STATE_BOOTING;
    Serial.begin(115200);
    Serial.println("Fractal Clock Simplified + Touch Starting...");

    // Init GFX
    Serial.println("Initializing GFX...");
    if (!gfx->begin()) { Serial.println("gfx->begin() failed!"); while (1); }
    w = gfx->width(); // Set global dimensions
    h = gfx->height();
    initializeColors(); // Initialize colors now that gfx is ready
    Serial.println("GFX Initialized.");
    Serial.printf("Display resolution: %d x %d\n", w, h);
    gfx->fillScreen(BLACK);

    // Init Touch
    Serial.println("Initializing I2C for Touch...");
    Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
    Serial.println("Initializing GT911 Touch Controller (TAMC_GT911)...");
    ts_ptr = new TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(w, h), min(w, h));
    if (!ts_ptr) { Serial.println("Failed touch alloc!"); while (1); }
    ts_ptr->begin();
    Serial.println("GT911 Initialized.");

    // Backlight
    #ifdef GFX_BL
        pinMode(GFX_BL, OUTPUT); digitalWrite(GFX_BL, HIGH); Serial.println("Backlight ON.");
    #endif

    // Network & Time Setup
    currentClockState = STATE_WAITING_FOR_WIFI;
    gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setFont(NULL);
    gfx->setCursor(10, h/2 -10); gfx->print("Connecting to WiFi...");
    connectToWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        currentClockState = STATE_WAITING_FOR_NTP;
        syncTimeNTP();
        if (timeSynchronized) {
            if (getLocalTime(&timeinfo, 5000)) {
                Serial.println("Initial time obtained and stored locally.");
                Serial.print("Current local time struct: ");
                Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
                lastNtpSyncMillis = millis();
                currentClockState = STATE_RUNNING_NTP;
                gfx->fillScreen(BLACK);
                displayStaticElements(&timeinfo);
            } else {
                Serial.println("Error: Failed to seed time after NTP sync!");
                timeSynchronized = false;
                currentClockState = STATE_SETTING_TIME;
            }
        } else {
            Serial.println("Initial NTP sync failed. Entering manual time set mode.");
            currentClockState = STATE_SETTING_TIME;
        }
    } else {
        Serial.println("WiFi connection failed. Entering manual time set mode.");
        currentClockState = STATE_SETTING_TIME;
        timeSynchronized = false;
    }

    if (currentClockState == STATE_SETTING_TIME) {
        Serial.println("Initializing time for manual setting.");
        timeinfo = {0}; timeinfo.tm_hour = 12; timeinfo.tm_min = 0; timeinfo.tm_sec = 0;
        timeinfo.tm_mday = 1; timeinfo.tm_mon = 0; timeinfo.tm_year = 124; // 2024
        mktime(&timeinfo); // Calculate initial weekday
        needsStaticRedraw = true;
        displaySetTimeScreen(&timeinfo);
    }

    previousMillis = millis();
}

// --- Loop ---
void loop() {
    unsigned long currentMillis = millis();

    // --- Read Touch Input ---
    bool istouched_now = false;
    int currentTouchX = -1, currentTouchY = -1; // Store current coords if touched
    if (ts_ptr) {
        ts_ptr->read();
        istouched_now = ts_ptr->isTouched;
        if (istouched_now && ts_ptr->touches > 0) {
            currentTouchX = ts_ptr->points[0].x;
            currentTouchY = ts_ptr->points[0].y;
            // Update touch visibility tracking
            lastDisplayedTouchX = currentTouchX;
            lastDisplayedTouchY = currentTouchY;
            lastTouchMillis = currentMillis; // Reset timeout timer
            if (!touchCoordsVisible) { // If it just became visible
                 touchCoordsVisible = true;
                 // Force redraw by making stored string different
                 previousTouchCoordsStr[0] = ' ';
                 previousTouchCoordsStr[1] = '\0';
            }
        }
    }

    // --- Main State Machine ---
    switch (currentClockState) {
        case STATE_SETTING_TIME:
            handleTimeSettingTouch(); // Handles touch actions and redraws set screen
            firstDisplayDone = false;
            break;

        case STATE_RUNNING_NTP:
            if (timeSynchronized && (currentMillis - lastNtpSyncMillis >= ntpSyncInterval)) {
                 resyncTimeNTP();
            }
            // Fall through

        case STATE_RUNNING_MANUAL:
            if (currentMillis - previousMillis >= interval) {
                previousMillis = currentMillis;
                if (timeSynchronized) {
                    time_t currentTimeSec = mktime(&timeinfo);
                    currentTimeSec += 1;
                    localtime_r(&currentTimeSec, &timeinfo);
                    if (needsStaticRedraw) {
                        Serial.println("Redrawing static elements after manual set.");
                        gfx->fillScreen(BLACK);
                        displayStaticElements(&timeinfo);
                        needsStaticRedraw = false;
                    }
                    displayClock(&timeinfo); // Update Time and Factors
                    firstDisplayDone = true;
                }
            }
            else if (timeSynchronized && !firstDisplayDone) {
                 if (needsStaticRedraw) {
                     gfx->fillScreen(BLACK);
                     displayStaticElements(&timeinfo);
                     needsStaticRedraw = false;
                 }
                 displayClock(&timeinfo);
                 firstDisplayDone = true;
            }
            break;

        // Other states (Waiting, Booting) are handled in setup or implicitly
        case STATE_WAITING_FOR_WIFI:
        case STATE_WAITING_FOR_NTP:
        case STATE_BOOTING:
            break; // Do nothing in loop for these states for now
    }

    // --- Display Touch Coordinates (independent of state, except SETTING_TIME maybe) ---
    if(currentClockState != STATE_SETTING_TIME) {
        displayTouchCoords();
    } else {
        // Ensure coords are cleared if we enter setting mode while they were visible
        if (touchCoordsVisible) {
             if (prev_touch_coords_w > 0) { // Clear if needed
                 gfx->fillRect(prev_touch_coords_x, prev_touch_coords_y, prev_touch_coords_w, prev_touch_coords_h, BLACK);
                 prev_touch_coords_w = 0;
             }
             touchCoordsVisible = false;
             previousTouchCoordsStr[0] = '\0';
        }
    }


    // Reset touch debounce flag when touch is released
    if (!istouched_now) {
        touchRegisteredThisPress = false;
    }

    yield(); // Yield for system tasks
}