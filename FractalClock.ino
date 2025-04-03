#include <WiFi.h>
#include <time.h>       // For NTP time and time functions
#include <freertos/FreeRTOS.h> // For FreeRTOS features
#include <freertos/task.h>     // For task management
#include <freertos/queue.h>    // For queues
#include <Arduino_GFX_Library.h>
#include "Arduino_GFX_dev_device.h" // Assumes this is correctly configured

// Font Includes - Make sure paths are correct for your system
#include "C:\Users\mcgreg\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSansBold18pt7b.h"
#include "C:\Users\mcgreg\Documents\Arduino\libraries\Adafruit_GFX_Library\Fonts\FreeSans18pt7b.h"

// --- WiFi Credentials ---
const char *ssid = "Pandora2";
const char *password = "LetMeIn#123";

// --- NTP Settings ---
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;       // GMT+1 offset
const int daylightOffset_sec = 3600; // DST offset
const long ntpSyncInterval = 60000; // How often to re-sync NTP (milliseconds), e.g., 60000 = 1 minute ADJUSTABLE
unsigned long lastNtpSyncMillis = 0;  // Timer for NTP re-sync

// --- Graphics Object (ensure this matches your setup) ---
#ifndef GFX_DEV_DEVICE
#error "Please configure your GFX device pins and constructor first!"
#endif

// --- Task & Queue Configuration ---
#define PRELOAD_COUNT 20       // How many seconds ahead to calculate initially & maintain
#define CACHE_SIZE (PRELOAD_COUNT + 5) // Size of the results cache (slightly larger than preload)
#define LOW_PRIORITY 1         // Priority for background task when queue is full
#define HIGH_PRIORITY 3        // Priority for background task when queue is low
#define LOW_QUEUE_THRESHOLD 5  // Trigger high priority if requests pending < this
#define FACTOR_TASK_STACK_SIZE 4096 // Stack size for the factorization task
#define MAX_FACTOR_STR_LEN 512 // Max length for factor string storage

// --- Data Structures for Queues ---
typedef struct {
    int numberToFactor;
} FactorRequest_t;

typedef struct {
    int originalNumber;
    char factorString[MAX_FACTOR_STR_LEN];
} FactorResult_t;

// --- Global Variables ---
int32_t w, h;                // Screen width and height
struct tm timeinfo;          // Structure to hold time information (master copy)
unsigned long previousMillis = 0; // For non-blocking display update (1-second tick)
const long interval = 1000;  // Update interval (1 second)
bool timeSynchronized = false; // Flag to know when NTP sync is done (initial or re-sync)

// German weekdays and months
const char *Wochentage[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char *Monate[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};

// Define colors
#define RGB565_LIGHT_GREY gfx->color565(170, 170, 170)

// --- RTOS Handles ---
QueueHandle_t requestQueue;
QueueHandle_t resultQueue;
TaskHandle_t factorTaskHandle = NULL;

// --- Result Cache ---
FactorResult_t resultCache[CACHE_SIZE];
int cacheWriteIndex = 0;
bool cacheInitialized = false;

// --- Function Declarations ---
void displayStaticElements(const struct tm* currentTime); // For drawing date/weekday once
void centerText(const char *text, int y, uint16_t color, const GFXfont *font /* = NULL */, uint8_t size /* = 1 */); // Default args removed from definition
void connectToWiFi();
void syncTimeNTP(); // Initial sync function
void resyncTimeNTP(); // Periodic re-sync function
void displayClock(const struct tm* currentTime); // Takes time as parameter
void factorTask(void *parameter);
bool getFactorsFromCache(int number, char *outString, size_t maxLen);
void addResultToCache(const FactorResult_t *result);
void requestFactorization(int number);
int calculateFutureTimeNum(int currentHour, int currentMin, int currentSec, int secondsToAdd);
int countValidCacheEntries(); // Declaration for the existing function

// --- Setup Function ---
void setup() {
#ifdef DEV_DEVICE_INIT
    DEV_DEVICE_INIT();
#endif

    Serial.begin(115200);
    Serial.println("Fractal Clock Starting...");

    // Initialize Cache
    for (int i = 0; i < CACHE_SIZE; ++i) {
        resultCache[i].originalNumber = -1; // Mark as empty
        resultCache[i].factorString[0] = '\0';
    }
    cacheInitialized = true;
    Serial.println("Result cache initialized.");

    // --- Create Queues ---
    requestQueue = xQueueCreate(PRELOAD_COUNT + 5, sizeof(FactorRequest_t));
    resultQueue = xQueueCreate(CACHE_SIZE, sizeof(FactorResult_t));
    if (requestQueue == NULL || resultQueue == NULL) {
        Serial.println("Error creating queues!"); while (1);
    }
    Serial.println("RTOS Queues created.");

    // --- Create Background Task ---
    xTaskCreatePinnedToCore(factorTask, "FactorTask", FACTOR_TASK_STACK_SIZE, NULL, LOW_PRIORITY, &factorTaskHandle, 0);
    if (factorTaskHandle == NULL) {
        Serial.println("Error creating background task!"); while (1);
    }
    Serial.println("Background factor task created.");

    // --- Initialize Graphics ---
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!"); while (1) delay(100);
    }
    Serial.println("GFX Initialized.");
    w = gfx->width();
    h = gfx->height();
    Serial.printf("Display resolution: %d x %d\n", w, h);

    // --- Connect WiFi and Perform Initial Time Sync ---
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    // Use simple print if centerText causes issues during connect
    gfx->setCursor(10, h/2 -10);
    gfx->print("Connecting to WiFi...");
    // centerText("Connecting to WiFi...", h / 2 - 10, RGB565_WHITE, NULL, 1); // Use if stable
    connectToWiFi(); // Note: centerText call inside might be commented out

    syncTimeNTP(); // Attempt initial synchronization, sets timeSynchronized flag

    // --- Seed Local Time & Preload if Initial Sync Succeeded ---
    if (timeSynchronized) {
        if (getLocalTime(&timeinfo, 5000)) { // Get the synchronized time into our global struct
            Serial.println("Initial time obtained and stored locally.");
            Serial.print("Current local time struct: ");
            Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
            lastNtpSyncMillis = millis(); // Start the NTP re-sync timer

            Serial.printf("Preloading next %d factor requests...\n", PRELOAD_COUNT);
            for (int i = 0; i < PRELOAD_COUNT; ++i) {
                // Use the seeded global timeinfo for preloading
                int futureNum = calculateFutureTimeNum(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, i + 1);
                requestFactorization(futureNum);
            }
            gfx->fillScreen(RGB565_BLACK); // Clear screen once before drawing static parts
            displayStaticElements(&timeinfo); // Draw Date/Weekday
            Serial.println("Preloading complete.");
        } else {
            Serial.println("Error: Failed to seed local time struct after initial sync!");
            timeSynchronized = false; // Mark as not properly synchronized
            gfx->fillScreen(RGB565_RED);
             gfx->setCursor(10, h/2 -10); gfx->print("Error: Time seed failed!"); delay(5000);
        }
    } else {
        Serial.println("Initial NTP sync failed. Clock will not run until sync succeeds.");
        // Keep the "NTP Sync Failed!" message displayed from syncTimeNTP()
    }

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    Serial.println("Backlight ON.");
#endif

    // Perform initial display only if time is ready
    if (timeSynchronized) {
        displayClock(&timeinfo); // Initial display using the seeded time
    }
    previousMillis = millis(); // Start the 1-second display timer regardless
}

// --- Main Loop (Core 1) ---
void loop() {
    unsigned long currentMillis = millis();
    static bool firstDisplayDone = false; // Track if initial dynamic display occurred

    // --- Process Results from Background Task ---
    FactorResult_t receivedResult;
    if (xQueueReceive(resultQueue, &receivedResult, 0) == pdPASS) {
        addResultToCache(&receivedResult); // Adds to cache and prints count
    }

    // --- Periodic NTP Re-sync Check ---
    // Check if time has been synchronized at least once and the interval has passed
    if (timeSynchronized && (currentMillis - lastNtpSyncMillis >= ntpSyncInterval)) {
         resyncTimeNTP();
         // lastNtpSyncMillis is updated inside resyncTimeNTP only on success,
         // ensuring it retries sooner if the sync fails.
    }

    // --- Update Display via Local Time Increment (Every Second) ---
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis; // Reset the 1-second tick timer

        if (timeSynchronized) {
            // --- Increment Local Time ---
            time_t currentTimeSec = mktime(&timeinfo); // Convert struct tm to seconds since epoch
            currentTimeSec += 1;                       // Add one second
            localtime_r(&currentTimeSec, &timeinfo);   // Convert back to struct tm (thread-safe), updating global timeinfo

            // --- Update Dynamic Parts of Display ---
            // This function now handles partial updates for time and factors
            displayClock(&timeinfo);
            firstDisplayDone = true; // Mark that dynamic display has run at least once

        } else {
            // --- Time is not synchronized ---
            firstDisplayDone = false; // Reset flag if sync is lost or not yet achieved
            gfx->fillScreen(RGB565_BLACK); // Clear screen for the waiting message
            gfx->setCursor(10, h/2); // Simple print for status
            gfx->setTextColor(RGB565_YELLOW);
            gfx->setTextSize(1);
            gfx->print("Waiting for initial NTP sync...");

            // Attempt initial sync again if WiFi is connected
            if(WiFi.status() == WL_CONNECTED) {
                Serial.println("Loop: Attempting initial sync again...");
                syncTimeNTP(); // Try the initial sync process again
                if(timeSynchronized) { // If successful now...
                     // Seed the local time struct IMMEDIATELY
                     if (getLocalTime(&timeinfo, 5000)) {
                         Serial.println("Initial time obtained in loop and stored locally.");
                         lastNtpSyncMillis = millis(); // Start NTP resync timer
                         // Preload again? Optional.
                         // Serial.printf("Preloading next %d factor requests...\n", PRELOAD_COUNT);
                         // for (int i = 0; i < PRELOAD_COUNT; ++i) { ... }
                         // Serial.println("Preloading complete.");

                         // Draw the initial static and dynamic parts immediately
                         gfx->fillScreen(RGB565_BLACK); // Clear waiting message
                         displayStaticElements(&timeinfo);
                         displayClock(&timeinfo);
                         firstDisplayDone = true;

                     } else {
                         Serial.println("Error: Failed to seed time after sync in loop!");
                         timeSynchronized = false; // Mark as failed again
                         gfx->fillScreen(RGB565_RED); gfx->setCursor(10, h/2);
                         gfx->print("Error: Time seed failed!"); delay(3000);
                     }
                }
                // If syncTimeNTP failed again, the waiting message will reappear on the next loop iteration
            } else {
                // Optional: Display WiFi disconnected message below waiting message
                gfx->setCursor(10, h/2 + 20);
                gfx->print("WiFi Disconnected. Cannot sync time.");
            }
        }
    }
    // --- Handle First Display Immediately After Setup ---
    // This covers the case where the loop runs after time is synced in setup,
    // but before the first one-second interval timer has elapsed.
    else if (timeSynchronized && !firstDisplayDone) {
         // Draw the dynamic parts for the first time
         displayClock(&timeinfo);
         firstDisplayDone = true;
    } // End 1-second tick or first display check

    // Yield to allow other tasks (important for WiFi, background task, etc.)
    yield();
}

// --- Helper Functions ---

void connectToWiFi() {
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attempts++;
        if (attempts > 20) {
            Serial.println("\nFailed to connect to WiFi!");
            gfx->fillScreen(RGB565_BLACK);
            // Use simple print if centerText causes crashes here
             gfx->setCursor(10, h/2 -10); gfx->setTextColor(RGB565_RED); gfx->setTextSize(1);
             gfx->print("WiFi Connection Failed!");
            // centerText("WiFi Connection Failed!", h / 2 - 10, RGB565_RED, NULL, 1);
            while (1) delay(1000);
        }
        yield(); // Add yield in loop
    }
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    gfx->fillScreen(RGB565_BLACK);
    // centerText("WiFi Connected!", h / 2 - 10, RGB565_WHITE, NULL, 1); // KEEP COMMENTED if unstable
    delay(1000); // Allow screen to show status briefly if uncommented
}

// Initial time synchronization - only sets the timeSynchronized flag
void syncTimeNTP() {
    Serial.println("Attempting initial time sync configuration...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    gfx->fillScreen(RGB565_BLACK);
    centerText("Syncing Time...", h / 2 - 10, RGB565_WHITE, NULL, 1); // Center text ok here usually
    Serial.print("Waiting for NTP time sync: ");

    struct tm checkTime; // Use a local struct for the check
    if (!getLocalTime(&checkTime, 15000)) { // Increased timeout for initial sync
        Serial.println("\nFailed to obtain time initially");
        centerText("NTP Sync Failed!", h / 2 - 10, RGB565_RED, NULL, 1);
        timeSynchronized = false; // Set flag to false
        delay(2000); // Show message
    } else {
         Serial.println("\nInitial Time Synchronization Flag Set");
         centerText("Time Synchronized", h / 2 - 10, RGB565_WHITE, NULL, 1);
         timeSynchronized = true; // Set flag to true
         delay(1000); // Show message
         // Print for confirmation, but don't seed the main timeinfo here
         Serial.print("NTP check successful, current time: ");
         Serial.println(&checkTime, "%A, %B %d %Y %H:%M:%S");
    }
}

// Periodic NTP re-sync - updates the global timeinfo struct on success
void resyncTimeNTP() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("NTP Re-sync skipped: WiFi not connected.");
        // Consider setting timeSynchronized = false here if accuracy is critical
        return;
    }

    Serial.print("Attempting periodic NTP re-sync... ");
    // Re-configure? It might help ensure the settings are fresh.
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm tempTimeInfo; // Use a temporary struct to check sync
    if (!getLocalTime(&tempTimeInfo, 5000)) { // Shorter timeout for re-sync
        Serial.println("Failed.");
        // Keep using the old locally incremented time. Don't necessarily mark as unsynchronized
        // unless it fails multiple times in a row (more complex logic).
    } else {
         Serial.println("Success.");
         // *** CRITICAL: Update the main global timeinfo struct ***
         timeinfo = tempTimeInfo;
         timeSynchronized = true; // Ensure flag is true
         lastNtpSyncMillis = millis(); // Reset the re-sync timer ONLY on success
         Serial.print("Time re-synchronized: ");
         Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
}


// --- Display Function ---
// MODIFIED: Updates only dynamic elements (Time, Factors) with partial erase
// Incorporates user-specified layout and improved full-width factor line clearing.
void displayClock(const struct tm* currentTime) {
    if (currentTime == NULL) {
        Serial.println("DisplayClock Error: Received NULL time pointer.");
        return; // Safety check
    }

    // --- Static variables to store previous values and dimensions ---
    static char previousTimeStr[9] = "";
    static char previousCombinedOutputStr[10 + MAX_FACTOR_STR_LEN] = "";
    // Store previous bounding box info for erasing
    static int16_t prev_time_y = -1; // Top Y pos of previous time draw
    static uint16_t prev_time_w = 0, prev_time_h = 0; // W/H of previous time
    // For combined line: store height and y1 offset of PREVIOUS text
    static uint16_t prev_comb_h = 0;  // Height of the previous text box
    static int16_t prev_comb_y1 = 0; // Relative y1 offset of previous text from its baseline

    // --- Fonts ---
    const GFXfont *mainFont = &FreeSansBold18pt7b;
    const GFXfont *subFont = &FreeSans18pt7b;

    // --- Calculate Font Heights (needed for layout) ---
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, subFontHeight;
    // Height for main font size 1 (Weekday, Date)
    gfx->setFont(mainFont); gfx->setTextSize(1);
    gfx->getTextBounds("M", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    // Height approximation for Time (main font size 2)
    int timeHeight = mainFontHeight * 2; // User's approximation for layout spacing
    // Get height for Sub Font size 1 (Factors line)
    gfx->setFont(subFont); gfx->setTextSize(1);
    gfx->getTextBounds("0", 0, 0, &temp_x, &temp_y, &temp_w, &subFontHeight);

    int lineSpacing = 10;

    // --- APPLY USER'S LAYOUT ---
    // Calculate total height needed for midY calculation
    // Using heights: main(1) + main(1) + time(approx 2*main(1)) + sub(1) + 3*spacing
    int totalHeight = (mainFontHeight * 2) + timeHeight + subFontHeight + (lineSpacing * 3);
    int midY = (h - totalHeight) / 2; // Calculate Mid Y for overall block centering
    if (midY < 0) midY = 0; // Prevent negative Y

    // Calculate Y positions for dynamic elements using user's logic
    // Static elements (weekday, date) are drawn elsewhere.
    int y_time = midY; // User's value for Time's top edge
    int y_timenum_factors = y_time + timeHeight + lineSpacing; // User's value for Factors top edge

    // --- Generate Current Strings ---
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", currentTime->tm_hour, currentTime->tm_min, currentTime->tm_sec);

    int timeNum = currentTime->tm_hour * 10000 + currentTime->tm_min * 100 + currentTime->tm_sec;
    char factorsStr[MAX_FACTOR_STR_LEN] = "Calculating...";
    getFactorsFromCache(timeNum, factorsStr, sizeof(factorsStr));
    char combinedOutputStr[10 + MAX_FACTOR_STR_LEN];
    snprintf(combinedOutputStr, sizeof(combinedOutputStr), "%06d=%s", timeNum, factorsStr);

    // --- Update Time (if changed) ---
    if (strcmp(timeStr, previousTimeStr) != 0) {
        // Erase previous time (centered)
        // Check if previous dimensions are valid (has been drawn at least once)
        if (prev_time_w > 0 && prev_time_h > 0 && prev_time_y >= 0) {
             // Calculate absolute X position of the previous bounding box
             int16_t prev_abs_x = (w - prev_time_w) / 2;
             // Erase using the stored Y position (top edge) and dimensions
             gfx->fillRect(prev_abs_x, prev_time_y, prev_time_w, prev_time_h, RGB565_BLACK);
        }

        // Draw new time (Centered, Large) using CURRENT y_time
        centerText(timeStr, y_time, RGB565_WHITE, mainFont, 2); // Size 2

        // Store new bounds and position for next erase cycle
        int16_t new_x1, new_y1; // Relative offsets, not needed for erase calc
        gfx->setFont(mainFont); gfx->setTextSize(2); // Ensure font/size set for getTextBounds
        // Get dimensions of the newly drawn text
        gfx->getTextBounds(timeStr, 0, 0, &new_x1, &new_y1, &prev_time_w, &prev_time_h);
        prev_time_y = y_time; // Store the top Y position that was used for this draw
        strcpy(previousTimeStr, timeStr); // Update the comparison string
    }

    // --- Update Combined Factors Line (if changed) ---
    if (strcmp(combinedOutputStr, previousCombinedOutputStr) != 0) {
        int left_margin = 4; // Must match the drawing margin

        // Erase previous combined line (left aligned - CLEAR FULL WIDTH)
        // Check if previous height is valid (indicates it was drawn at least once)
        if (prev_comb_h > 0) {
             // Calculate the baseline Y that was used for the PREVIOUS draw.
             // This assumes the overall layout determining y_timenum_factors is stable.
             int prev_cursor_y_comb = y_timenum_factors + 40;

             // Calculate the absolute TOP Y coordinate where the PREVIOUS text's
             // bounding box started, using the previous baseline and previous y1 offset.
             int prev_top_y = prev_cursor_y_comb + prev_comb_y1; // prev_comb_y1 is likely negative

             // Clear from X=0 to full width 'w'.
             // Start at the calculated previous top Y edge 'prev_top_y'.
             // Use the stored height of the previous text line 'prev_comb_h'.
             gfx->fillRect(0,           // Start X
                           prev_top_y,  // Start Y (actual top of previous text)
                           w,           // Width (full screen)
                           prev_comb_h, // Height (of previous text)
                           RGB565_BLACK); // Background color
        }

        // Draw new combined line (Left Aligned)
        gfx->setFont(subFont);
        gfx->setTextSize(1); // Size 1 for factors line
        int16_t x1_comb, y1_comb; // Relative offsets for the NEW text
        uint16_t w_comb, h_comb; // Width/Height for the NEW text
        // Get bounds of the NEW text being drawn
        gfx->getTextBounds(combinedOutputStr, 0, 0, &x1_comb, &y1_comb, &w_comb, &h_comb);

        // --- APPLY USER'S Y-calculation for baseline ---
        int cursor_y_comb = y_timenum_factors + 40;

        // Draw the new text
        gfx->setCursor(left_margin, cursor_y_comb);
        gfx->setTextColor(RGB565_WHITE);
        gfx->print(combinedOutputStr);

        // Store info about the NEWLY drawn text for the NEXT erase cycle
        strcpy(previousCombinedOutputStr, combinedOutputStr); // Update comparison string
        prev_comb_h = h_comb;     // Store height of the current text
        prev_comb_y1 = y1_comb;   // Store y1 offset of the current text relative to its baseline
        // prev_comb_w is not needed for full-width clearing

    } // End if combinedOutputStr changed


    // --- Request factorization for a future second ---
    // Use the passed currentTime for calculation
    int futureNumToRequest = calculateFutureTimeNum(currentTime->tm_hour, currentTime->tm_min, currentTime->tm_sec, PRELOAD_COUNT);
    requestFactorization(futureNumToRequest);
}

// --- Factorization Task (Core 0) ---
// (No changes needed in factorTask itself)
void factorTask(void *parameter) {
    Serial.println("FactorTask started.");
    FactorRequest_t request;
    FactorResult_t result;
    char localBuf[20]; // Local buffer for sprintf
    UBaseType_t currentPriority = uxTaskPriorityGet(NULL); // Get initial priority

    while (1) {
        // --- Adjust Priority Based on Queue Level ---
        UBaseType_t requestsWaiting = uxQueueMessagesWaiting(requestQueue);
        if (requestsWaiting < LOW_QUEUE_THRESHOLD && currentPriority != HIGH_PRIORITY) {
            vTaskPrioritySet(NULL, HIGH_PRIORITY);
            currentPriority = HIGH_PRIORITY;
            Serial.println("FactorTask: Priority HIGH");
        } else if (requestsWaiting >= LOW_QUEUE_THRESHOLD && currentPriority != LOW_PRIORITY) {
            vTaskPrioritySet(NULL, LOW_PRIORITY);
            currentPriority = LOW_PRIORITY;
            Serial.println("FactorTask: Priority LOW");
        }

        // --- Wait for a request ---
        if (xQueueReceive(requestQueue, &request, portMAX_DELAY) == pdPASS) {
            int n = request.numberToFactor;
            result.originalNumber = n;
            result.factorString[0] = '\0';

            // --- Perform Factorization ---
            if (n <= 0) { strcpy(result.factorString, "0"); }
            else if (n == 1) { strcpy(result.factorString, "1"); }
            else {
                while (n % 2 == 0) {
                    strcat(result.factorString, "2*");
                    n /= 2; yield();
                }
                for (int i = 3; i * i <= n; i += 2) {
                     while (n % i == 0) {
                        sprintf(localBuf, "%d*", i);
                        strcat(result.factorString, localBuf);
                        n /= i; yield();
                    }
                }
                if (n > 1) {
                    sprintf(localBuf, "%d*", n);
                    strcat(result.factorString, localBuf);
                }
                int len = strlen(result.factorString);
                if (len > 0 && result.factorString[len - 1] == '*') {
                    result.factorString[len - 1] = '\0'; // Remove trailing '*'
                }
            } // End factorization logic

            // --- Send Result Back ---
            xQueueSend(resultQueue, &result, pdMS_TO_TICKS(100)); // Ignore failure for now
        }
    } // End while(1)
}


// --- Cache Management Functions ---
// Adds a result to the cache AND prints current count
void addResultToCache(const FactorResult_t *result) {
    if (!cacheInitialized || result == NULL) return;

    resultCache[cacheWriteIndex].originalNumber = result->originalNumber;
    strncpy(resultCache[cacheWriteIndex].factorString, result->factorString, MAX_FACTOR_STR_LEN - 1);
    resultCache[cacheWriteIndex].factorString[MAX_FACTOR_STR_LEN - 1] = '\0';

    cacheWriteIndex = (cacheWriteIndex + 1) % CACHE_SIZE;

    // --- Count and Print ---
    int validCount = countValidCacheEntries();
    //Serial.printf("Cache: Stored %d. Total valid entries: %d/%d\n", result->originalNumber, validCount, CACHE_SIZE);
}

// Searches cache for a number's factors (no changes)
bool getFactorsFromCache(int number, char *outString, size_t maxLen) {
    if (!cacheInitialized || outString == NULL || maxLen == 0) return false;
    for (int i = 0; i < CACHE_SIZE; ++i) {
        if (resultCache[i].originalNumber == number) {
            strncpy(outString, resultCache[i].factorString, maxLen - 1);
            outString[maxLen - 1] = '\0';
            return true;
        }
    }
    return false;
}

// Counts the number of valid entries currently in the result cache (no changes)
int countValidCacheEntries() {
    if (!cacheInitialized) return 0;
    int validCount = 0;
    for (int i = 0; i < CACHE_SIZE; ++i) {
        if (resultCache[i].originalNumber != -1) {
            validCount++;
        }
    }
    return validCount;
}


// --- Request Helper --- (no changes)
void requestFactorization(int number) {
    FactorRequest_t req;
    req.numberToFactor = number;
    xQueueSend(requestQueue, &req, 0); // Send non-blocking
}

// --- Time Calculation Helper --- (no changes)
int calculateFutureTimeNum(int currentHour, int currentMin, int currentSec, int secondsToAdd) {
    long totalSeconds = currentHour * 3600 + currentMin * 60 + currentSec;
    totalSeconds += secondsToAdd;
    totalSeconds %= 86400;
    if (totalSeconds < 0) { totalSeconds += 86400; }
    int futureHour = totalSeconds / 3600;
    int futureMin = (totalSeconds % 3600) / 60;
    int futureSec = totalSeconds % 60;
    return futureHour * 10000 + futureMin * 100 + futureSec;
}


// --- Centering Text Function ---
// Definition - NO default arguments here
void centerText(const char *text, int y, uint16_t color, const GFXfont *font, uint8_t size) {
    if (!text || strlen(text) == 0) return;
    int16_t x1, y1;
    uint16_t text_w, text_h;
    gfx->setFont(font);
    gfx->setTextSize(size);
    gfx->getTextBounds(text, 0, 0, &x1, &y1, &text_w, &text_h);
    int cursor_x = (w - text_w) / 2 - x1;
    int cursor_y = y - y1; // Adjust for top alignment
    if (cursor_x < 0) cursor_x = 0;
    gfx->setCursor(cursor_x, cursor_y);
    gfx->setTextColor(color);
    gfx->print(text);
}
// --- Display Static Elements Function ---
// Draws elements that don't change every second
// --- Display Static Elements Function ---
// Draws elements that don't change every second
void displayStaticElements(const struct tm* currentTime) {
    if (currentTime == NULL) return;

    const GFXfont *mainFont = &FreeSansBold18pt7b;
    const GFXfont *subFont = &FreeSans18pt7b; // Needed for height calculation

    // --- Calculate Font Heights ---
    int16_t temp_x, temp_y;
    uint16_t temp_w, mainFontHeight, subFontHeight;
    gfx->setFont(mainFont); gfx->setTextSize(1);
    gfx->getTextBounds("M", 0, 0, &temp_x, &temp_y, &temp_w, &mainFontHeight);
    int timeHeightApprox = mainFontHeight * 2; // Approx height for Time (size 2)
    gfx->setFont(subFont); gfx->setTextSize(1);
    gfx->getTextBounds("0", 0, 0, &temp_x, &temp_y, &temp_w, &subFontHeight);

    int lineSpacing = 10;

    // --- APPLY USER'S LAYOUT ---
    // Calculate total height needed for midY calculation
    int totalHeight = (mainFontHeight * 2) + timeHeightApprox + subFontHeight + (lineSpacing * 3);
    int midY = (h - totalHeight) / 2; // Calculate Mid Y based on overall block

    // Calculate Y positions using user's logic
    int y_weekday = 12; // User's fixed value
    int y_date = y_weekday + mainFontHeight + lineSpacing;
    // y_time and y_timenum_factors are calculated dynamically later, but midY is needed now.

    // --- Draw Weekday and Date (Centered) ---
    char weekdayStr[20];
    snprintf(weekdayStr, sizeof(weekdayStr), "%s,", Wochentage[currentTime->tm_wday]);
    centerText(weekdayStr, y_weekday, RGB565_LIGHT_GREY, mainFont, 1);

    char dateStr[40];
    snprintf(dateStr, sizeof(dateStr), "%d. %s %d", currentTime->tm_mday, Monate[currentTime->tm_mon], currentTime->tm_year + 1900);
    centerText(dateStr, y_date, RGB565_LIGHT_GREY, mainFont, 1);
}