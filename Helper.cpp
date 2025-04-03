#include "CustomDef.h" // Include defines and globals
#include "Helper.h"    // Include function declarations

// Define colors that need the gfx object
bool firstDisplayDone = false;
uint16_t RGB565_LIGHT_GREY;
uint16_t COLOR_OK_BUTTON_GREEN;

// WiFi (DEFINITION WITH VALUE)
const char *ssid = "Pandora2";
const char *password = "LetMeIn#123";

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



// Initialization helper
void initializeColors() {
    RGB565_LIGHT_GREY = gfx->color565(170, 170, 170);
    COLOR_OK_BUTTON_GREEN = gfx->color565(0, 180, 0);
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
      snprintf(buffer, sizeof(buffer), "%s,", Wochentage[currentTime->tm_wday]);
      snprintf(buffer, sizeof(buffer), "%d. %s %d", currentTime->tm_mday, Monate[currentTime->tm_mon], currentTime->tm_year + 1900);
      centerText(buffer, y_date, RGB565_LIGHT_GREY, font_freesansbold18, 1);
    }
}

// Updates dynamic elements (Time, Factors)
void displayClock(const struct tm* currentTime) {
    if (currentTime == NULL) { return; }
    if (currentClockState == STATE_SETTING_TIME) return;

    // Static vars for erase tracking...
    static char previousTimeStr[9] = "";
    static int16_t prev_time_y = -1;
    static uint16_t prev_time_w = 0, prev_time_h = 0;
    static char previousFactorStr[MAX_FACTOR_STR_LEN] = ""; // Store previous factor string
    static uint16_t prev_factor_h = 0;
    static int16_t prev_factor_y1 = 0;

    // --- Fonts ---
    const GFXfont *timeFont = font_freesansbold18; // Size 2 for drawing
    const GFXfont *factorFont = font_freesans18;  // Size 1 for drawing

    // --- Calculate Font Heights ---
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, factorFontHeight, rssDummyHeight;
    gfx->setFont(timeFont); gfx->setTextSize(1); // Get base height
    gfx->getTextBounds("M", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    int timeHeight = mainFontHeight * 2; // Approx height for Time (size 2)
    gfx->setFont(factorFont); gfx->setTextSize(1);
    gfx->getTextBounds("0", 0, 0, &temp_x, &temp_y, &temp_w, &factorFontHeight);
    gfx->setFont(font_freesans12); gfx->setTextSize(1); // Dummy RSS height
    gfx->getTextBounds("X", 0, 0, &temp_x, &temp_y, &temp_w, &rssDummyHeight);

    // --- Define Spacing ---
    int lineSpacing = 10;
    int spacing_factors_to_dummy = 40; // Keep consistent spacing

    // --- Calculate Total Height & Centering (based on 5 potential lines) ---
    int totalHeight = (mainFontHeight * 2) + timeHeight + factorFontHeight + rssDummyHeight +
                      (lineSpacing * 3) + spacing_factors_to_dummy;
    int startY = (h - totalHeight) / 2; if (startY < 5) startY = 5;

    // --- Calculate Y positions for dynamic elements ---
    int y_time_top_target = startY + (mainFontHeight * 2) + (lineSpacing * 2); // Target based on static elements
    int y_factor_top_target = y_time_top_target + timeHeight + lineSpacing; // Target for factor line

    // --- Generate Current Time String ---
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", currentTime->tm_hour, currentTime->tm_min, currentTime->tm_sec);

    // --- Calculate Factors for CURRENT time ---
    int timeNum = currentTime->tm_hour * 10000 + currentTime->tm_min * 100 + currentTime->tm_sec;
    char factorBuffer[MAX_FACTOR_STR_LEN];
    primeFactorsToString(timeNum, factorBuffer, sizeof(factorBuffer)); // Calculate directly
    char combinedOutputStr[10 + MAX_FACTOR_STR_LEN];
    snprintf(combinedOutputStr, sizeof(combinedOutputStr), "%06d=%s", timeNum, factorBuffer);

    // --- Update Time Display ---
    if (strcmp(timeStr, previousTimeStr) != 0) {
        if (prev_time_w > 0 && prev_time_h > 0 && prev_time_y >= 0) {
             int16_t prev_abs_x = (w - prev_time_w) / 2;
             gfx->fillRect(prev_abs_x, prev_time_y, prev_time_w, prev_time_h, BLACK);
        }
        centerText(timeStr, y_time_top_target, WHITE, timeFont, 2); // Draw large time
        int16_t new_x1, new_y1;
        gfx->setFont(timeFont); gfx->setTextSize(2);
        gfx->getTextBounds(timeStr, 0, 0, &new_x1, &new_y1, &prev_time_w, &prev_time_h);
        prev_time_y = y_time_top_target; // Store Y top edge used
        strcpy(previousTimeStr, timeStr);
    }

    // --- Update Factors Display ---
    if (strcmp(combinedOutputStr, previousFactorStr) != 0) {
        int left_margin = 4;
        if (prev_factor_h > 0) { // Clear previous
            // Simplified clear: Use target Y and previous height
             gfx->fillRect(0, y_factor_top_target, w, prev_factor_h, BLACK);
        }
        // Draw new factors line
        gfx->setFont(factorFont); gfx->setTextSize(1); gfx->setTextColor(WHITE);
        int16_t x1, y1; uint16_t w1, h1;
        gfx->getTextBounds(combinedOutputStr, 0, 0, &x1, &y1, &w1, &h1);
        int cursor_y = y_factor_top_target - y1; // Align top edge
        gfx->setCursor(left_margin, cursor_y);
        gfx->print(combinedOutputStr);
        // Store info for next erase
        strcpy(previousFactorStr, combinedOutputStr);
        prev_factor_h = h1;
        prev_factor_y1 = y1; // Not used in simplified clear, but store anyway
    }
}


// Displays the manual time setting interface
void displaySetTimeScreen(const struct tm* currentTimeToDisplay) {
     gfx->fillScreen(BLACK);

    // --- Fonts ---
    const GFXfont *mainFont = font_freesansbold18; // Large time
    const GFXfont *promptFont = font_freesans18; // Instructions
    const GFXfont *buttonFont = font_freesansbold18; // OK button

    // --- Calculate Layout ---
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, promptFontHeight, buttonFontHeight;
    gfx->setFont(mainFont); gfx->setTextSize(2);
    gfx->getTextBounds("00:00:00", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    gfx->setFont(promptFont); gfx->setTextSize(1);
    gfx->getTextBounds("A", 0, 0, &temp_x, &temp_y, &temp_w, &promptFontHeight);
    gfx->setFont(buttonFont); gfx->setTextSize(1);
    gfx->getTextBounds("OK", 0, 0, &temp_x, &temp_y, &temp_w, &buttonFontHeight);

    int lineSpacing = 15;
    int totalHeight = mainFontHeight + promptFontHeight + OK_BUTTON_H + (lineSpacing * 2);
    int startY = (h - totalHeight) / 2; if (startY < 5) startY = 5;
    int y_time_set = startY;
    int y_prompt = y_time_set + mainFontHeight + lineSpacing;
    int y_ok_button = y_prompt + promptFontHeight + lineSpacing;

    // --- Draw Time ---
    char timeSet[9];
    snprintf(timeSet, sizeof(timeSet), "%02d:%02d:00", currentTimeToDisplay->tm_hour, currentTimeToDisplay->tm_min);
    centerText(timeSet, y_time_set, WHITE, mainFont, 2);

    // --- Draw Prompt ---
    centerText("Bitte Uhrzeit einstellen", y_prompt, RGB565_LIGHT_GREY, promptFont, 1);

    // --- Draw OK Button ---
    gfx->fillRoundRect(OK_BUTTON_X, y_ok_button, OK_BUTTON_W, OK_BUTTON_H, 5, COLOR_OK_BUTTON_GREEN);
    gfx->setTextColor(BLACK); gfx->setFont(buttonFont); gfx->setTextSize(1);
    gfx->getTextBounds("OK", 0, 0, &temp_x, &temp_y, &temp_w, &buttonFontHeight);
    int ok_text_cursor_x = OK_BUTTON_X + (OK_BUTTON_W - temp_w) / 2 - temp_x;
    int ok_text_cursor_y = y_ok_button + (OK_BUTTON_H - buttonFontHeight) / 2 - temp_y;
    gfx->setCursor(ok_text_cursor_x, ok_text_cursor_y);
    gfx->print("OK");
}

// Draws the current touch coordinates with timeout
void displayTouchCoords() {
    char currentCoordsStr[20];
    bool needsRedraw = false;
    bool clearNeeded = false;

    if (touchCoordsVisible) {
        // Check for timeout
        if (millis() - lastTouchMillis > touchDisplayTimeout) {
            touchCoordsVisible = false;
            clearNeeded = true; // Need to clear the last drawn coords
            strcpy(currentCoordsStr, ""); // Ensure comparison string is empty
        } else {
            // Format current visible coords
            snprintf(currentCoordsStr, sizeof(currentCoordsStr), "T:%3d,%3d", lastDisplayedTouchX, lastDisplayedTouchY);
            // Check if redraw is needed (content hasn't changed, but maybe it was cleared?)
             if(strcmp(currentCoordsStr, previousTouchCoordsStr) != 0 || prev_touch_coords_w == 0) {
                 needsRedraw = true;
             }
        }
    } else {
        // Ensure comparison string is empty when not visible
        strcpy(currentCoordsStr, "");
        if(prev_touch_coords_w > 0) { // If something was drawn before, clear it
            clearNeeded = true;
        }
    }

     // Compare with previous string to decide on action
     if (strcmp(currentCoordsStr, previousTouchCoordsStr) != 0 || needsRedraw || clearNeeded) {
         // --- 1. Clear Previous ---
         if (prev_touch_coords_w > 0) {
            gfx->fillRect(prev_touch_coords_x, prev_touch_coords_y, prev_touch_coords_w, prev_touch_coords_h, BLACK);
            prev_touch_coords_w = 0; // Mark as cleared
         }

         // --- 2. Draw New (if visible) ---
         if (touchCoordsVisible) {
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
         } else {
             // Reset bounds if not visible (already cleared)
             prev_touch_coords_w = 0;
             prev_touch_coords_h = 0;
         }
         strcpy(previousTouchCoordsStr, currentCoordsStr); // Update comparison string
     }
}

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


// --- Touch Handling ---
void handleTimeSettingTouch() {
    if (!ts_ptr || !ts_ptr->isTouched || ts_ptr->touches <= 0) {
        touchRegisteredThisPress = false; return;
    }
    if (touchRegisteredThisPress) return; // Debounce

    int touchX = ts_ptr->points[0].x;
    int touchY = ts_ptr->points[0].y;
    bool needsRedraw = false;

    // --- Calculate dynamic Y positions for touch zones ---
    const GFXfont *mainFont = font_freesansbold18;
    const GFXfont *promptFont = font_freesans18;
    const GFXfont *buttonFont = font_freesansbold18;
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, promptFontHeight, buttonFontHeight;
    gfx->setFont(mainFont); gfx->setTextSize(2);
    gfx->getTextBounds("00:00:00", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    gfx->setFont(promptFont); gfx->setTextSize(1);
    gfx->getTextBounds("A", 0, 0, &temp_x, &temp_y, &temp_w, &promptFontHeight);
    gfx->setFont(buttonFont); gfx->setTextSize(1);
    gfx->getTextBounds("OK", 0, 0, &temp_x, &temp_y, &temp_w, &buttonFontHeight);
    int lineSpacing = 15;
    int totalHeight = mainFontHeight + promptFontHeight + OK_BUTTON_H + (lineSpacing * 2);
    int startY = (h - totalHeight) / 2; if (startY < 5) startY = 5;
    int y_time_set_top = startY;
    int time_touch_zone_Y = y_time_set_top + TIME_SET_Y_OFFSET; // Approx below time display
    int y_ok_button_top = y_time_set_top + mainFontHeight + lineSpacing + promptFontHeight + lineSpacing;

    // --- Check Touch Zones ---
    if (touchX >= HOUR_TOUCH_X && touchX < (HOUR_TOUCH_X + HOUR_TOUCH_W) &&
        touchY >= time_touch_zone_Y && touchY < (time_touch_zone_Y + TIME_SET_TOUCH_H)) {
        Serial.println("Hour Zone Touched");
        timeinfo.tm_hour = (timeinfo.tm_hour + 1) % 24; needsRedraw = true; touchRegisteredThisPress = true;
    } else if (touchX >= MINUTE_TOUCH_X && touchX < (MINUTE_TOUCH_X + MINUTE_TOUCH_W) &&
             touchY >= time_touch_zone_Y && touchY < (time_touch_zone_Y + TIME_SET_TOUCH_H)) {
        Serial.println("Minute Zone Touched");
        timeinfo.tm_min = (timeinfo.tm_min + 1) % 60; needsRedraw = true; touchRegisteredThisPress = true;
    } else if (touchX >= OK_BUTTON_X && touchX < (OK_BUTTON_X + OK_BUTTON_W) &&
             touchY >= y_ok_button_top && touchY < (y_ok_button_top + OK_BUTTON_H)) {
        Serial.println("OK Button Touched");
        timeinfo.tm_sec = 0; mktime(&timeinfo); // Normalize time (calculates weekday)
        currentClockState = STATE_RUNNING_MANUAL; timeSynchronized = true; needsStaticRedraw = true;
        firstDisplayDone = false; touchRegisteredThisPress = true;
    }

    if (needsRedraw) {
        displaySetTimeScreen(&timeinfo); // Redraw setting screen with new time
    }
}


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