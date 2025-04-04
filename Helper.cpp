#include "CustomDef.h" // Include defines and globals
#include "Helper.h"    // Include function declarations


// Define colors that need the gfx object
bool firstDisplayDone = false;
uint16_t RGB565_LIGHT_GREY;
uint16_t COLOR_OK_BUTTON_GREEN;
uint16_t COLOR_DEBUG_CYAN;
uint16_t COLOR_BUTTON_GREY;

// WiFi (DEFINITION WITH VALUE)
const char *ssid = "Pandora2";
const char *password = "Let2MeIn#123";

// NTP (DEFINITION WITH VALUE)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const long ntpSyncInterval = 15 * 60 * 1000; // 15 minutes

// Timekeeping (DEFINITION WITH VALUE)
const long interval = 1000;

// Define font pointers
const GFXfont *font_freesansbold18 = &FreeSansBold18pt7b;
const GFXfont *font_freesans18 = &FreeSans18pt7b;
const GFXfont *font_freesans12 = &FreeSans12pt7b;

// German weekdays and months definitions
const char *Wochentage[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char *Monate[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

const unsigned long touchDisplayTimeout = 3000; // 3 seconds (or your desired value)

// --- Time Setting Layout Globals ---
int set_time_y_top = 330;
int set_time_touch_y_top = 330;
int set_time_hour_x_left = 370;
int set_time_minute_x_left = 270;
int set_time_box_width = 70;   // Default/initial width, can be adjusted
int set_time_box_height = 70;   // Default/initial height
int set_time_ok_y_top = 200;
int set_time_ok_x_left = 310;


// Initialization helper
void initializeColors() {
    RGB565_LIGHT_GREY = gfx->color565(170, 170, 170);
    COLOR_OK_BUTTON_GREEN = gfx->color565(0, 180, 0);
    COLOR_DEBUG_CYAN = gfx->color565(0, 255, 255); // Initialize the debug color
        COLOR_BUTTON_GREY = gfx->color565(100, 100, 100); // Define grey color
    set_time_ok_x_left = (w - OK_BUTTON_W) / 2;
}

// --- WiFi Connection ---
void connectToWiFi() {
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        if (attempts++ > 20) {
            Serial.println("\nFailed to connect to WiFi!");
            gfx->fillScreen(BLACK);
            gfx->setCursor(10, h/2 -10); gfx->setTextColor(RED); gfx->setTextSize(1); gfx->setFont(NULL);
            gfx->print("WiFi Connection Failed!");
            delay(3000);
            return; // Let setup proceed
        }
        delay(500); Serial.print("."); yield();
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    gfx->fillScreen(BLACK);
    gfx->setCursor(10, h/2 -10); gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setFont(NULL);
    gfx->print("WiFi Connected!");
    delay(1000);
}

// --- NTP Synchronization ---
void syncTimeNTP() {
    Serial.println("Attempting initial time sync configuration...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    gfx->fillScreen(BLACK);
    gfx->setCursor(10, h/2 -10); gfx->setTextColor(WHITE); gfx->setTextSize(1); gfx->setFont(NULL);
    gfx->print("Syncing Time...");
    Serial.print("Waiting for NTP time sync: ");
    struct tm checkTime;
    if (!getLocalTime(&checkTime, 15000)) {
        Serial.println("\nFailed to obtain time initially");
        timeSynchronized = false;
        gfx->fillScreen(BLACK);
        gfx->setCursor(10, h/2 -10); gfx->setTextColor(RED); gfx->print("NTP Sync Failed!");
        delay(2000);
    } else {
         Serial.println("\nInitial Time Synchronization Flag Set");
         timeSynchronized = true;
         gfx->fillScreen(BLACK);
         gfx->setCursor(10, h/2 -10); gfx->setTextColor(GREEN); gfx->print("Time Synchronized!");
         delay(1000);
         Serial.print("NTP check successful, current time: ");
         Serial.println(&checkTime, "%A, %B %d %Y %H:%M:%S");
    }
}

void resyncTimeNTP() {
    if (currentClockState != STATE_RUNNING_NTP) { return; }
    if (WiFi.status() != WL_CONNECTED) {
         Serial.println("NTP Re-sync skipped: WiFi not connected.");
         return;
    }
    Serial.print("Attempting periodic NTP re-sync... ");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm tempTimeInfo;
    if (!getLocalTime(&tempTimeInfo, 5000)) {
        Serial.println("Failed.");
    } else {
         Serial.println("Success.");
         timeinfo = tempTimeInfo; // Update main time struct
         timeSynchronized = true;
         lastNtpSyncMillis = millis();
         Serial.print("Time re-synchronized: ");
         Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
         needsStaticRedraw = true; // Date/weekday might have changed
    }
}

// --- Display Functions ---

// Draws elements that don't change every second
void displayStaticElements(const struct tm* currentTime) {
     if (currentTime == NULL) return;

    // --- Calculate Font Heights ---
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, subFontHeight, rssFontHeight; // Keep rssFontHeight for layout consistency if needed later
    gfx->setFont(font_freesansbold18); gfx->setTextSize(1);
    gfx->getTextBounds("M", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    int timeHeightApprox = mainFontHeight * 2;
    gfx->setFont(font_freesans18); gfx->setTextSize(1);
    gfx->getTextBounds("0", 0, 0, &temp_x, &temp_y, &temp_w, &subFontHeight); // Height for Factors
    gfx->setFont(font_freesans12); gfx->setTextSize(1); // Use 12pt for dummy RSS height calc
    gfx->getTextBounds("X", 0, 0, &temp_x, &temp_y, &temp_w, &rssFontHeight); // Dummy height

    // --- Define Spacing ---
    int lineSpacing = 10;
    int spacing_factors_to_dummy = 40; // Keep spacing consistent

    // --- Calculate Total Height for Centering (based on 5 potential lines) ---
    int totalHeight = (mainFontHeight * 2) + timeHeightApprox + subFontHeight + rssFontHeight +
                      (lineSpacing * 3) + spacing_factors_to_dummy;
    int startY = (h - totalHeight) / 2; if (startY < 5) startY = 5;

    // --- Calculate Y positions ---
    int y_weekday = startY;
    int y_date = y_weekday + mainFontHeight + lineSpacing;

    if (currentClockState == STATE_RUNNING_NTP) {

      char buffer[40];
      snprintf(buffer, sizeof(buffer), "%s,\n", Wochentage[currentTime->tm_wday]);
      centerText(buffer, 10, RGB565_LIGHT_GREY, font_freesansbold18, 1);
      snprintf(buffer, sizeof(buffer), "%d. %s %d", currentTime->tm_mday, Monate[currentTime->tm_mon], currentTime->tm_year + 1900);
      centerText(buffer, 20+mainFontHeight, RGB565_LIGHT_GREY, font_freesansbold18, 1);
    }
}

// In Helper.cpp

// ... (includes and other functions) ...

// In Helper.cpp

void displayClock(const struct tm* currentTime) {
    if (currentTime == NULL) {
        Serial.println("DisplayClock Error: Received NULL time pointer.");
        return;
    }
    // Don't draw if in time setting mode
    if (currentClockState == STATE_SETTING_TIME) {
        return;
    }

    // Static vars for erase tracking
    static char previousTimeStr[9] = "";
    static int16_t prev_time_y = -1; // Stores top Y where previous time was drawn
    static uint16_t prev_time_w = 0, prev_time_h = 0; // Stores dimensions of previous time
    static char previousFactorStr[MAX_FACTOR_STR_LEN] = ""; // Stores previous combined factor string
    static uint16_t prev_factor_h = 0;  // Stores height of previous factor string box
    static int16_t prev_factor_y1 = 0; // Stores relative y1 offset of previous factor string

    // --- Fonts ---
    const GFXfont *mainFont = font_freesansbold18; // For Time (Size 2)
    const GFXfont *subFont = font_freesans18;   // For Factors line (Size 1)

    // --- Calculate Font Heights needed for Layout ---
    int16_t temp_x_t, temp_y_t, temp_x_f, temp_y_f; // Temporary vars for bounds
    uint16_t temp_w_t, time_h, temp_w_f, factor_h, mainFontHeight; // Store heights
    // Get base height of main font (size 1) to calculate time height and static spacing
    gfx->setFont(mainFont); gfx->setTextSize(1);
    gfx->getTextBounds("M", 0, 0, &temp_x_t, &temp_y_t, &temp_w_t, &mainFontHeight);
    // Get actual height of Time text (main font size 2)
    gfx->setFont(mainFont); gfx->setTextSize(2);
    gfx->getTextBounds("00:00:00", 0, 0, &temp_x_t, &temp_y_t, &temp_w_t, &time_h);
    // Get actual height of Factors text (sub font size 1)
    gfx->setFont(subFont); gfx->setTextSize(1);
    gfx->getTextBounds("0", 0, 0, &temp_x_f, &temp_y_f, &temp_w_f, &factor_h);

    // --- Calculate Vertical Layout for the 4 Lines ---
    int lineSpacing = 10;
    // Total height: Weekday(main1) + Date(main1) + Time(size2) + Factors(sub1) + 3 * spacing
    int totalHeight = (mainFontHeight * 2) + time_h + factor_h + (lineSpacing * 3);
    int startY = (h - totalHeight) / 2; // Center the entire 4-line block vertically
    if (startY < 5) startY = 5; // Ensure minimum top margin

    // Calculate Y positions for the TOP edge of dynamic elements
    // Based on the positions of the static elements calculated from startY
    int y_time_top_target = 170;//startY + (mainFontHeight * 2) + (lineSpacing * 2); // FIXME: 
    int y_factor_top_target = y_time_top_target + time_h + lineSpacing;       // Below Time line

    // --- Generate Current Time & Factor Strings ---
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", currentTime->tm_hour, currentTime->tm_min, currentTime->tm_sec);
    int timeNum = currentTime->tm_hour * 10000 + currentTime->tm_min * 100 + currentTime->tm_sec;
    char factorBuffer[MAX_FACTOR_STR_LEN];
    primeFactorsToString(timeNum, factorBuffer, sizeof(factorBuffer)); // Calculate directly
    char combinedOutputStr[10 + MAX_FACTOR_STR_LEN];
    snprintf(combinedOutputStr, sizeof(combinedOutputStr), "%06d=%s", timeNum, factorBuffer);

    // --- Update Time Display (Centered, Large) ---
    if (strcmp(timeStr, previousTimeStr) != 0) {
        // 1. Erase Previous Time
        if (prev_time_w > 0 && prev_time_h > 0 && prev_time_y >= 0) {
             int16_t prev_abs_x = (w - prev_time_w) / 2; // Center X
             gfx->fillRect(prev_abs_x, prev_time_y, prev_time_w, prev_time_h, BLACK);
        }
        // 2. Draw New Time
        centerText(timeStr, y_time_top_target, WHITE, mainFont, 2); // Draw at calculated Y target

        // 3. Store Bounds for Next Erase
        int16_t new_x1, new_y1;
        gfx->setFont(mainFont); gfx->setTextSize(2); // Ensure correct font/size for bounds
        gfx->getTextBounds(timeStr, 0, 0, &new_x1, &new_y1, &prev_time_w, &prev_time_h);
        prev_time_y = y_time_top_target; // Store the Top Y edge used for drawing
        strcpy(previousTimeStr, timeStr);
    }

    // --- Update Factors Display (Left Aligned) ---
    if (strcmp(combinedOutputStr, previousFactorStr) != 0) {
        int left_margin = 4;
        // 1. Erase Previous Factors Line (Full Width)
        if (prev_factor_h > 0) {
             // Clear using the target Y top and the previous height
             gfx->fillRect(0, y_factor_top_target, w, prev_factor_h, BLACK);
        }
        // 2. Draw New Factors Line
        gfx->setFont(subFont); gfx->setTextSize(1); gfx->setTextColor(WHITE);
        int16_t x1_comb, y1_comb; uint16_t w1_comb, h1_comb;
        gfx->getTextBounds(combinedOutputStr, 0, 0, &x1_comb, &y1_comb, &w1_comb, &h1_comb);
        int cursor_y = y_factor_top_target - y1_comb; // Calculate baseline to align top edge
        gfx->setCursor(left_margin, cursor_y);
        gfx->print(combinedOutputStr);

        // 3. Store Bounds for Next Erase
        strcpy(previousFactorStr, combinedOutputStr);
        prev_factor_h = h1_comb; // Store height of the text just drawn
        prev_factor_y1 = y1_comb; // Store baseline offset (not used in current clear)
    }

} // End displayClock


// Centers text horizontally, aligns top edge vertically
void centerText(const char *text, int y, uint16_t color, const GFXfont *font, uint8_t size) {
    if (!text || strlen(text) == 0) return;
    int16_t x1, y1; uint16_t text_w, text_h;
    gfx->setFont(font); gfx->setTextSize(size);
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &text_w, &text_h);
    int cursor_x = (w - text_w) / 2 - x1; int cursor_y = y - y1;
    if (cursor_x < 0) cursor_x = 0;
    gfx->setCursor(cursor_x, cursor_y); gfx->setTextColor(color); gfx->print(text);
}

// In Helper.cpp

#include "CustomDef.h" // Includes defines and globals
#include "Helper.h"    // Includes function declarations


// IP Address Display Tracking
char previousIPInfoString[50] = ""; // <<< ADDED definition
uint16_t prev_ip_x = 0;          // <<< ADDED definition
uint16_t prev_ip_y = 0;          // <<< ADDED definition
uint16_t prev_ip_w = 0;          // <<< ADDED definition
uint16_t prev_ip_h = 0;          // <<< ADDED definition


// --- Display Time Setting Screen ---
// Calculates layout dynamically, assumes standard GFX coordinates (0,0 Top-Left)
// Draws debug boxes shifted left and closer vertically to the time display.
void displaySetTimeScreen(const struct tm* currentTimeToDisplay) {
    gfx->fillScreen(BLACK);

    // --- Fonts ---
    const GFXfont *mainFont = font_freesansbold18; // Large time
    const GFXfont *promptFont = font_freesans18;   // Instructions
    const GFXfont *buttonFont = font_freesansbold18; // OK button

    // --- Calculate Font Heights ---
    int16_t temp_x_t, temp_y_t, temp_x_p, temp_y_p, temp_x_b, temp_y_b;
    uint16_t temp_w_t, time_h, temp_w_p, prompt_h, temp_w_b, button_h; // Specific height vars
    gfx->setFont(mainFont); gfx->setTextSize(2);
    gfx->getTextBounds("00:00:00", 0, 0, &temp_x_t, &temp_y_t, &temp_w_t, &time_h); // Gets time_h
    gfx->setFont(promptFont); gfx->setTextSize(1);
    gfx->getTextBounds("A", 0, 0, &temp_x_p, &temp_y_p, &temp_w_p, &prompt_h); // Gets prompt_h
    gfx->setFont(buttonFont); gfx->setTextSize(1);
    gfx->getTextBounds("OK", 0, 0, &temp_x_b, &temp_y_b, &temp_w_b, &button_h); // Gets button_h

    int lineSpacing = 15; // Space between main elements

    // --- Calculate Centered Layout ---
    // Height includes Time, Prompt, OK Button, and spacing
    int totalHeight = time_h + prompt_h + OK_BUTTON_H + (lineSpacing * 2);
    int startY = (h - totalHeight) / 2; // Top Y of the entire block
    if (startY < 5) startY = 5;

    int y_time_set_target_top = startY; // Top edge target for centerText for the time

    // --- Calculate ACTUAL draw position of time text ---
    int time_actual_x = (w - temp_w_t) / 2; // Use time width (temp_w_t) for centering
    int time_actual_y = y_time_set_target_top; // Actual Top edge Y where time text bounding box starts

    // --- Draw Large Time (HH:MM:00) ---
    char timeSet[9];
    snprintf(timeSet, sizeof(timeSet), "%02d:%02d:00", currentTimeToDisplay->tm_hour, currentTimeToDisplay->tm_min);
    centerText(timeSet, y_time_set_target_top, WHITE, mainFont, 2); // Draw using centerText

    // --- Calculate Y positions for elements BELOW the time ---
    int y_prompt = time_actual_y + time_h + lineSpacing; // Position below actual time height
    int y_ok_button = y_prompt + prompt_h + lineSpacing; // Position below prompt

    // --- Draw Prompt Text ---
    centerText("Bitte Uhrzeit einstellen", y_prompt, RGB565_LIGHT_GREY, promptFont, 1);

    // --- Draw OK Button ---
    gfx->fillRoundRect(OK_BUTTON_X, y_ok_button, OK_BUTTON_W, OK_BUTTON_H, 5, COLOR_OK_BUTTON_GREEN);
    gfx->setTextColor(BLACK); gfx->setFont(buttonFont); gfx->setTextSize(1);
    int ok_text_cursor_x = OK_BUTTON_X + (OK_BUTTON_W - temp_w_b) / 2 - temp_x_b;
    int ok_text_cursor_y = y_ok_button + (OK_BUTTON_H - button_h) / 2 - temp_y_b;
    gfx->setCursor(ok_text_cursor_x, ok_text_cursor_y);
    gfx->print("OK");


    int original_boxes_y_top = time_actual_y + time_h + 5;

    int boxes_y_top = time_actual_y + time_h -55;//THIS IS IMPORTANT!!!

    // Use the previously calculated shifted X positions
    int touch_box_width = 100; // Keep width for now, adjust if needed
    int shift_left_amount = 40;
    // int time_actual_x = (w - temp_w_t) / 2; // Already calculated above
    int hour_box_x_orig = time_actual_x + (temp_w_t * 0.1);
    int minute_section_width = temp_w_t * 0.4;
    int minute_box_x_orig = time_actual_x + temp_w_t - minute_section_width - (temp_w_t * 0.1);
    minute_box_x_orig = max(0, minute_box_x_orig);
    int hour_box_x = max(0, hour_box_x_orig - shift_left_amount);
    int minute_box_x = max(0, minute_box_x_orig - shift_left_amount);

    gfx->drawRect(hour_box_x, boxes_y_top, touch_box_width, TIME_SET_TOUCH_H, COLOR_DEBUG_CYAN);
    gfx->drawRect(minute_box_x, boxes_y_top, touch_box_width, TIME_SET_TOUCH_H, COLOR_DEBUG_CYAN);

    // Print boundaries for debugging
    Serial.printf("DRAW Hour Box: X=%d, Y=%d, W=%d, H=%d\n", hour_box_x, boxes_y_top, touch_box_width, TIME_SET_TOUCH_H);
    Serial.printf("DRAW Min Box : X=%d, Y=%d, W=%d, H=%d\n", minute_box_x, boxes_y_top, touch_box_width, TIME_SET_TOUCH_H);
    Serial.printf("DRAW OK Box  : X=%d, Y=%d, W=%d, H=%d\n", OK_BUTTON_X, y_ok_button, OK_BUTTON_W, OK_BUTTON_H);
}


// --- Handle Touch Input During Time Setting ---
void handleTimeSettingTouch() {
    if (!ts_ptr || !ts_ptr->isTouched || ts_ptr->touches <= 0) {
        touchRegisteredThisPress = false; return;
    }
    if (touchRegisteredThisPress) return; // Debounce

    // --- Get RAW touch coordinates & translate to Standard GFX Coords ---
    int rawTouchX = ts_ptr->points[0].x;
    int rawTouchY = ts_ptr->points[0].y;
    // <<< FIX: Ensure translated variables are declared and assigned >>>
    int touchX = (w - 1) - rawTouchX; // Translated X (0=Left)
    int touchY = (h - 1) - rawTouchY; // Translated Y (0=Top)
    Serial.printf("Raw: %d,%d => GFX: %d,%d | ", rawTouchX, rawTouchY, touchX, touchY);

    bool needsRedraw = false;

    // --- Recalculate element positions (MATCHING displaySetTimeScreen) ---
    // Get Time dimensions first
    const GFXfont *mainFont = font_freesansbold18;
    char timeSet[9];
    snprintf(timeSet, sizeof(timeSet), "%02d:%02d:00", timeinfo.tm_hour, timeinfo.tm_min);
    int16_t time_x1, time_y1; uint16_t time_w, time_h;
    gfx->setFont(mainFont); gfx->setTextSize(2);
    gfx->getTextBounds(timeSet, 0, 0, &time_x1, &time_y1, &time_w, &time_h);

    // Get Prompt/Button heights
    const GFXfont *promptFont = font_freesans18;
    const GFXfont *buttonFont = font_freesansbold18;
    int16_t temp_x_p, temp_y_p, temp_x_b, temp_y_b;
    uint16_t temp_w_p, temp_h_p, temp_w_b, temp_h_b;
    gfx->setFont(promptFont); gfx->setTextSize(1);
    gfx->getTextBounds("A", 0, 0, &temp_x_p, &temp_y_p, &temp_w_p, &temp_h_p); // prompt_h = temp_h_p
    gfx->setFont(buttonFont); gfx->setTextSize(1);
    gfx->getTextBounds("OK", 0, 0, &temp_x_b, &temp_y_b, &temp_w_b, &temp_h_b); // button_h = temp_h_b

    int lineSpacing = 15;
    int totalHeightApprox = time_h + temp_h_p + OK_BUTTON_H + (lineSpacing * 2);
    int startY = (h - totalHeightApprox) / 2; if (startY < 5) startY = 5;
    int time_actual_y = startY; // Top Y of time text bounding box

    // --- Define Hit Zone Boundaries using STANDARD GFX Coordinates - ADJUSTED Y ---
    // Y boundaries
    int original_time_touch_zone_Y_top = time_actual_y + time_h + 5; // Original position (5px below time)
    int time_touch_zone_Y_top = original_time_touch_zone_Y_top - 70; // Adjust upwards
    if (time_touch_zone_Y_top < 0) time_touch_zone_Y_top = 0; // Prevent negative Y
    int time_touch_zone_Bottom = time_touch_zone_Y_top + TIME_SET_TOUCH_H; // Bottom edge uses adjusted top + height

    // X boundaries (Match drawing logic with shift)
    int touch_box_width = 100;
    int shift_left_amount = 40;
    int time_actual_x = (w - time_w) / 2;
    int hour_touch_X_orig = time_actual_x + (time_w * 0.1);
    int minute_section_width = time_w * 0.4;
    int minute_touch_X_orig = time_actual_x + time_w - minute_section_width - (time_w * 0.1);
    minute_touch_X_orig = max(0, minute_touch_X_orig);
    int hour_touch_X_left = max(0, hour_touch_X_orig - shift_left_amount);
    int hour_touch_X_right = hour_touch_X_left + touch_box_width;
    int minute_touch_X_left = max(0, minute_touch_X_orig - shift_left_amount);
    int minute_touch_X_right = minute_touch_X_left + touch_box_width;

    // OK Button boundaries
    int y_prompt = time_actual_y + time_h + lineSpacing;
    int y_ok_button_top = y_prompt + temp_h_p + lineSpacing;
    int y_ok_button_bottom = y_ok_button_top + OK_BUTTON_H;
    int ok_button_X_left = OK_BUTTON_X;
    int ok_button_X_right = OK_BUTTON_X + OK_BUTTON_W;


    // Print calculated hit zones for debugging
    // Serial.printf("Hit Hour: X[%d-%d] Y[%d-%d] | ", hour_touch_X_left, hour_touch_X_right, time_touch_zone_Y_top, time_touch_zone_Bottom);
    // Serial.printf("Hit Min : X[%d-%d] Y[%d-%d] | ", minute_touch_X_left, minute_touch_X_right, time_touch_zone_Y_top, time_touch_zone_Bottom);
    // Serial.printf("Hit OK  : X[%d-%d] Y[%d-%d]\n", ok_button_X_left, ok_button_X_right, y_ok_button_top, y_ok_button_bottom);


    // --- Check Touch Zones using TRANSLATED Coordinates (touchX, touchY) and FINAL boundaries ---
    // Hour Zone
    if (touchX >= hour_touch_X_left && touchX < hour_touch_X_right &&
        touchY >= time_touch_zone_Y_top && touchY < time_touch_zone_Bottom) {
        Serial.println("ACTION: Hour Zone Touched");
        timeinfo.tm_hour = (timeinfo.tm_hour + 1) % 24;
        needsRedraw = true;
        touchRegisteredThisPress = true;
    }
    // Minute Zone
    else if (touchX >= minute_touch_X_left && touchX < minute_touch_X_right &&
             touchY >= time_touch_zone_Y_top && touchY < time_touch_zone_Bottom) {
        Serial.println("ACTION: Minute Zone Touched");
        timeinfo.tm_min = (timeinfo.tm_min + 1) % 60;
        needsRedraw = true;
        touchRegisteredThisPress = true;
    }
    // OK Button Zone
    else if (touchX >= ok_button_X_left && touchX < ok_button_X_right &&
             touchY >= y_ok_button_top && touchY < y_ok_button_bottom) {
        Serial.println("ACTION: OK Button Touched");
        mktime(&timeinfo);
        currentClockState = STATE_RUNNING_MANUAL;
        timeSynchronized = true; needsStaticRedraw = true; firstDisplayDone = false;
        touchRegisteredThisPress = true;
    } else {
        Serial.println("ACTION: Touch outside defined zones.");
    }

    // Redraw Setting Screen if Hour/Minute Changed
    if (needsRedraw) {
        displaySetTimeScreen(&timeinfo);
    }
} // End handleTimeSettingTouch
// ... (Rest of Helper.cpp functions: displayClock, displayStaticElements, primeFactorsToString, etc.) ...


// --- Time & Math ---
int calculateFutureTimeNum(int currentHour, int currentMin, int currentSec, int secondsToAdd) {
    long totalSeconds = currentHour * 3600 + currentMin * 60 + currentSec;
    totalSeconds += secondsToAdd; totalSeconds %= 86400;
    if (totalSeconds < 0) totalSeconds += 86400;
    int futureHour = totalSeconds / 3600; int futureMin = (totalSeconds % 3600) / 60; int futureSec = totalSeconds % 60;
    return futureHour * 10000 + futureMin * 100 + futureSec;
}

// Calculates prime factors and formats them into the buffer
void primeFactorsToString(int n, char* buffer, size_t bufferSize) {
    if (buffer == NULL || bufferSize == 0) return;
    buffer[0] = '\0'; // Start with an empty string
    char tempBuf[20]; // For converting numbers to string segments

    // Handle 0 and 1 directly
    if (n <= 0) { strncpy(buffer, "0", bufferSize - 1); buffer[bufferSize-1] = '\0'; return; }
    if (n == 1) { strncpy(buffer, "1", bufferSize - 1); buffer[bufferSize-1] = '\0'; return; }

    size_t currentLen = 0;
    bool firstFactor = true;

    // Handle factor 2
    while (n % 2 == 0) {
        if (!firstFactor) { strncat(buffer, "*", bufferSize - currentLen - 1); currentLen++; }
        strncat(buffer, "2", bufferSize - currentLen - 1); currentLen++;
        firstFactor = false;
        n /= 2;
        if (currentLen >= bufferSize - 2) goto end_factorization; // Check buffer space
    }

    // Check odd factors
    for (int i = 3; i * i <= n; i += 2) {
        while (n % i == 0) {
             if (!firstFactor) { strncat(buffer, "*", bufferSize - currentLen - 1); currentLen++; }
             snprintf(tempBuf, sizeof(tempBuf), "%d", i);
             strncat(buffer, tempBuf, bufferSize - currentLen - 1);
             currentLen = strlen(buffer); // Update length after potential multi-digit factor
             firstFactor = false;
             n /= i;
             if (currentLen >= bufferSize - (strlen(tempBuf) + 1) ) goto end_factorization; // Check buffer space
        }
    }

    // Remaining prime
    if (n > 1) {
         if (!firstFactor) { strncat(buffer, "*", bufferSize - currentLen - 1); currentLen++; }
         snprintf(tempBuf, sizeof(tempBuf), "%d", n);
         strncat(buffer, tempBuf, bufferSize - currentLen - 1);
    }

end_factorization:
    buffer[bufferSize - 1] = '\0'; // Ensure null termination
}

// --- COMPLETE FUNCTION ---
// Draws the current touch coordinates with timeout in top-left corner
void displayTouchCoords() {
    char currentCoordsStr[20]; // Buffer for "T:XXX,YYY"
    bool needsClear = false;
    bool needsDraw = false;

    // --- Determine current state ---
    if (touchCoordsVisible) {
        // Check for timeout first
        if (millis() - lastTouchMillis > touchDisplayTimeout) {
            // Timeout expired
            touchCoordsVisible = false;
            if (strlen(previousTouchCoordsStr) > 0) { // Only clear if something was there
                needsClear = true;
            }
            currentCoordsStr[0] = '\0'; // Target state is empty
        } else {
            // Still visible, format the string to potentially draw
            snprintf(currentCoordsStr, sizeof(currentCoordsStr), "T:%3d,%3d", lastDisplayedTouchX, lastDisplayedTouchY);
            // Check if the content changed OR if the area was previously cleared
            if (strcmp(currentCoordsStr, previousTouchCoordsStr) != 0 || prev_touch_coords_w == 0) {
                 needsDraw = true; // Need to draw the new/existing coords
                 if (strlen(previousTouchCoordsStr) > 0) {
                     needsClear = true; // Clear old ones first if content changed
                 }
            }
        }
    } else {
        // Not visible, ensure it's cleared if it was visible before
        currentCoordsStr[0] = '\0'; // Target state is empty
        if (strlen(previousTouchCoordsStr) > 0) { // If previous string wasn't empty
            needsClear = true;
        }
    }

     // --- Perform Actions ---
     if (needsClear) {
         if (prev_touch_coords_w > 0) { // Check if dimensions are valid
            gfx->fillRect(prev_touch_coords_x, prev_touch_coords_y, prev_touch_coords_w, prev_touch_coords_h, BLACK);
            prev_touch_coords_w = 0; // Mark as cleared visually
         }
     }

     if (needsDraw) { // Only draw if state is visible and needs update
         gfx->setFont(NULL); // Default font
         gfx->setTextSize(1); // Small size
         gfx->setTextColor(YELLOW);

         int16_t x1, y1; uint16_t w1, h1;
         gfx->getTextBounds(currentCoordsStr, 0, 0, &x1, &y1, &w1, &h1);

         uint16_t drawAreaX = 0; // Top-left corner
         uint16_t drawAreaY = 0;

         int cursorX = drawAreaX - x1;
         int cursorY = drawAreaY - y1; // Align top edge

         gfx->setCursor(cursorX, cursorY);
         gfx->print(currentCoordsStr);

         // Store bounds for next clear
         prev_touch_coords_x = cursorX + x1;
         prev_touch_coords_y = cursorY + y1;
         prev_touch_coords_w = w1;
         prev_touch_coords_h = h1;
     }

     // Update comparison string for next cycle
     strcpy(previousTouchCoordsStr, currentCoordsStr);
}