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
//const unsigned long touchDisplayTimeout = 3000; // ms
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
// --- Custom Display Initialization Sequence ---
static const uint8_t custom_st7701_init_operations[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    WRITE_COMMAND_8, 0xEF,
    WRITE_BYTES, 1, 0x08,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02, // VBP
    WRITE_C8_D16, 0xC2, 0x21, 0x08,
    WRITE_C8_D8, 0xCD, 0x00,

    WRITE_COMMAND_8, 0xB0, // Positive Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,

    WRITE_COMMAND_8, 0xB1, // Negative Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE_C8_D8, 0xB0, 0x60, // 5d
    WRITE_C8_D8, 0xB1, 0x30, // VCOM amplitude setting
    WRITE_C8_D8, 0xB2, 0x87, // VGH Voltage setting, 12V
    WRITE_C8_D8, 0xB3, 0x80,

    WRITE_C8_D8, 0xB5, 0x49, // VGL Voltage setting, -8.3V

    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21,

    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,

//    WRITE_C8_D8, 0xD0, 0x88,

    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x1B, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11,
    0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44,
    
    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 13,
    0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE4, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16,
    0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE7, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16,
    0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7,
    0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,

    WRITE_COMMAND_8, 0xEC,
    WRITE_BYTES, 2,
    0x3C, 0x00,

    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16,
    0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA,

//    WRITE_COMMAND_8, 0xEF,
//    WRITE_BYTES, 6,
//    0x10, 0x0D, 0x04, 0x08,
//    0x3F, 0x1F,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0x3A,
    WRITE_BYTES, 1,
    0x66,

    WRITE_COMMAND_8, 0x36,
    WRITE_BYTES, 1,
    0x08,

    WRITE_COMMAND_8, 0x11, // Sleep Out
    END_WRITE,

    DELAY, 120,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29, // Display On
    END_WRITE,

    DELAY, 50};
const int custom_st7701_init_operations_len = sizeof(custom_st7701_init_operations);

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
    bus, GFX_NOT_DEFINED /* RST */, custom_st7701_init_operations, custom_st7701_init_operations_len); // Use defined length
WiFiManager wifiManager; // <<< ADDED

// In FractalClock.ino

// --- WiFi Connection Function (using WiFiManager) ---
bool connectOrConfigureWiFi() {
    Serial.println("Attempting WiFi connection...");
    String hostname = "FractalClock";
    wifiManager.setHostname(hostname.c_str());
    wifiManager.setConnectTimeout(15);
    // wifiManager.setConfigPortalTimeout(180);

    // Custom Status Message Callback (Displays AP info on screen)
    wifiManager.setAPCallback([](WiFiManager *mgr) {
        Serial.println("WiFiManager: Entered config mode");
        Serial.print("AP Name: "); Serial.println(mgr->getConfigPortalSSID());
        Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); // Usually 192.168.4.1

        // --- Display AP info on screen ---
        gfx->fillScreen(BLACK);
        gfx->setFont(NULL); // Default font
        gfx->setTextSize(2); gfx->setTextColor(WHITE);
        gfx->setCursor(10, 50);
        gfx->println("WiFi Setup Mode"); // Title

        gfx->setTextSize(1); // Smaller text for info
        gfx->setCursor(10, 100);
        gfx->print("AP Name: ");
        gfx->println(mgr->getConfigPortalSSID()); // Show AP name to connect to

        gfx->setCursor(10, 120);
        gfx->print("Connect & Go To:");

        gfx->setCursor(10, 140);
        gfx->setTextColor(YELLOW); gfx->setTextSize(2);
        gfx->print("192.168.4.1"); // <<< Display the portal IP clearly

        gfx->setTextColor(RGB565_LIGHT_GREY); gfx->setTextSize(1);
        gfx->setCursor(10, 180); // Position below IP
        gfx->print("(Configure WiFi Network)"); // Add instruction

        // <<< THIS IS WHERE THE MESSAGE IS SHOWN DURING AP MODE >>>
        // No need to draw it continuously in displayClock()
    });

    Serial.println("Starting WiFiManager autoConnect...");
    if (!wifiManager.autoConnect("FractalClockSetup")) { // AP name if portal starts
        Serial.println("WiFiManager: Failed to connect and hit timeout");
        gfx->fillScreen(BLACK); // Clear AP mode message
        gfx->setCursor(10, h/2 -10); gfx->setTextColor(RED); gfx->setTextSize(1); gfx->setFont(NULL);
        gfx->print("WiFi Config Failed/Timeout!");
        delay(3000);
        return false;
    }

    // Connected successfully
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    gfx->fillScreen(BLACK); // Clear AP mode message
    gfx->setCursor(10, h/2 -10); gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setFont(NULL);
    gfx->print("WiFi Connected!");
    delay(1500);
    return true;
}


// --- Setup ---
void setup() {
    currentClockState = STATE_BOOTING;
    Serial.begin(115200);
    Serial.println("Fractal Clock + WiFiManager Starting...");

    // Init GFX & Touch
    Serial.println("Initializing GFX...");
    if (!gfx->begin()) { Serial.println("gfx->begin() failed!"); while (1); }
    w = gfx->width(); h = gfx->height();
    initializeColors(); // Init colors after GFX is ready
    Serial.println("GFX Initialized."); Serial.printf("Display: %dx%d\n", w, h);
    gfx->fillScreen(BLACK);
    Serial.println("Initializing Touch...");
    Wire.begin(TOUCH_GT911_SDA, TOUCH_GT911_SCL);
    ts_ptr = new TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(w, h), min(w, h));
    if (!ts_ptr) { Serial.println("Failed touch alloc!"); while (1); }
    ts_ptr->begin();
    Serial.println("GT911 Initialized.");
    #ifdef GFX_BL
        pinMode(GFX_BL, OUTPUT); digitalWrite(GFX_BL, HIGH); Serial.println("Backlight ON.");
    #endif

    // --- Network and Time Setup using WiFiManager ---
    currentClockState = STATE_WAITING_FOR_WIFI; // Tentative state

    wifiManager.resetSettings(); // FIXME

    if (connectOrConfigureWiFi()) { // Try connecting or start config portal
        // WiFi is Connected! Proceed with NTP
        currentClockState = STATE_WAITING_FOR_NTP;
        syncTimeNTP(); // Attempt NTP sync

        if (timeSynchronized) { // NTP Success
            if (getLocalTime(&timeinfo, 5000)) {
                Serial.println("Initial time obtained and stored locally.");
                lastNtpSyncMillis = millis();
                currentClockState = STATE_RUNNING_NTP;
                gfx->fillScreen(BLACK); // Clear previous messages
                displayStaticElements(&timeinfo);
            } else { // Seeding failed
                Serial.println("Error: Failed to seed time after NTP sync!");
                timeSynchronized = false;
                currentClockState = STATE_SETTING_TIME; // Fallback
            }
        } else { // NTP Failed
            Serial.println("Initial NTP sync failed. Entering manual time set mode.");
            currentClockState = STATE_SETTING_TIME;
        }
    } else { // WiFiManager failed (timeout or couldn't connect)
        Serial.println("WiFi connection failed via WiFiManager. Entering manual time set mode.");
        currentClockState = STATE_SETTING_TIME;
        timeSynchronized = false;
    }

    // --- Initialize Manual Time Set if Needed ---
    if (currentClockState == STATE_SETTING_TIME) {
        Serial.println("Initializing time for manual setting.");
        timeinfo = {0}; timeinfo.tm_hour = 12; timeinfo.tm_min = 0; timeinfo.tm_sec = 0;
        timeinfo.tm_mday = 1; timeinfo.tm_mon = 0; timeinfo.tm_year = 124; // 2024
        mktime(&timeinfo); // Calculate initial weekday
        needsStaticRedraw = true;
        displaySetTimeScreen(&timeinfo);
    }

    previousMillis = millis(); // Start the 1-second display timer
}

// --- Loop ---
void loop() {
    unsigned long currentMillis = millis();
    // bool firstDisplayDone = false; // Now global

    // --- Read Touch Input ---
    bool istouched_now = false;
    int currentTouchX = -1, currentTouchY = -1;
    if (ts_ptr) {
        ts_ptr->read();
        istouched_now = ts_ptr->isTouched;
        if (istouched_now && ts_ptr->touches > 0) {
            currentTouchX = ts_ptr->points[0].x;
            currentTouchY = ts_ptr->points[0].y;
            lastDisplayedTouchX = currentTouchX; lastDisplayedTouchY = currentTouchY;
            lastTouchMillis = currentMillis;
            if (!touchCoordsVisible) {
                 touchCoordsVisible = true; previousTouchCoordsStr[0] = ' '; previousTouchCoordsStr[1] = '\0';
            }
        }
    }

    // --- Main State Machine ---
    switch (currentClockState) {
        case STATE_SETTING_TIME:
            handleTimeSettingTouch();
            firstDisplayDone = false;
            break;

        case STATE_RUNNING_NTP:
            if (timeSynchronized && (currentMillis - lastNtpSyncMillis >= ntpSyncInterval)) {
                 resyncTimeNTP();
            }
            // Fall through

        case STATE_RUNNING_MANUAL:
            if (currentMillis - previousMillis >= interval) { // 1-second tick
                previousMillis = currentMillis;
                if (timeSynchronized) {
                    time_t currentTimeSec = mktime(&timeinfo);
                    currentTimeSec += 1;
                    localtime_r(&currentTimeSec, &timeinfo);
                    if (needsStaticRedraw) {
                        Serial.println("Redrawing static elements.");
                        gfx->fillScreen(BLACK);
                        displayStaticElements(&timeinfo);
                        needsStaticRedraw = false;
                    }
                    displayClock(&timeinfo); // Update Time, Factors, IP
                    firstDisplayDone = true;
                }
            }
            else if (timeSynchronized && !firstDisplayDone) { // Initial draw after setup/OK
                 if (needsStaticRedraw) {
                     gfx->fillScreen(BLACK); displayStaticElements(&timeinfo); needsStaticRedraw = false;
                 }
                 displayClock(&timeinfo);
                 firstDisplayDone = true;
            }
            break;

        case STATE_WAITING_FOR_WIFI: // Handled by WiFiManager blocking call
        case STATE_WAITING_FOR_NTP:  // Transitions in setup
        case STATE_BOOTING:
            // Wait or display specific message if needed, setup handles transitions
            break;
    }

    // --- Display Touch Coordinates ---
    if(currentClockState != STATE_SETTING_TIME) {
        displayTouchCoords();
    } else { // Ensure cleared if entering set mode
        if (touchCoordsVisible) {
             if (prev_touch_coords_w > 0) { gfx->fillRect(prev_touch_coords_x, prev_touch_coords_y, prev_touch_coords_w, prev_touch_coords_h, BLACK); prev_touch_coords_w = 0; }
             touchCoordsVisible = false; previousTouchCoordsStr[0] = '\0';
        }
    }

    // Reset touch debounce flag when touch is released
    if (!istouched_now) {
        touchRegisteredThisPress = false;
    }

    yield(); // Yield for system tasks
}