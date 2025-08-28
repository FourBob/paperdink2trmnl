#include "paperdink_hardware.h"
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_mac.h>

// Display driver includes - using generic GxEPD2 for compatibility
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <PNGdec.h>

// Configure a concrete 4.2" B/W EPD (400x300) using GxEPD2
// Pins are defined in config.h
static GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> epd(GxEPD2_420(EPD_CS_PIN, EPD_DC_PIN, EPD_RESET_PIN, EPD_BUSY_PIN));

// Global PNG decoder instance for callback access
static PNG s_png;

struct PngDrawContext {
    int x0;
    int y0;
    int tW;
    int tH;
    float sX;
    float sY;
    bool invert;
};

// PNGdec draw callback: render each decoded line directly to the EPD with scaling (nearest neighbor)
static int pngDrawToEPD(PNGDRAW *pDraw) {
    PngDrawContext *ctx = (PngDrawContext *)pDraw->pUser;

    // Buffers for one decoded source line and one 1-bit destination line across display width
    static uint16_t line565[1024];
    static uint8_t lineBits[(DISPLAY_WIDTH + 7) / 8];

    if (pDraw->iWidth > 1024) {
        // Too wide for our temporary buffer; abort decode to avoid overflow
        return 0; // stops decode
    }

    // Convert current source line to RGB565
    s_png.getLineAsRGB565(pDraw, line565, PNG_RGB565_LITTLE_ENDIAN, 0xFFFFFFFF);

    // Clear destination line to white
    memset(lineBits, 0xFF, sizeof(lineBits));

    const int srcW = pDraw->iWidth;
    const int dstW = ctx->tW;
    const float sX = ctx->sX;

    // Build scaled destination lineBits for this source line
    for (int dx = 0; dx < dstW; ++dx) {
        int sx = (int)(dx / sX);
        if (sx < 0) sx = 0;
        if (sx >= srcW) sx = srcW - 1;
        uint16_t c = line565[sx];
        // Convert RGB565 to luma and threshold to 1-bit (with optional invert)
        uint8_t r5 = (c >> 11) & 0x1F;
        uint8_t g6 = (c >> 5) & 0x3F;
        uint8_t b5 = (c) & 0x1F;
        uint8_t r = (r5 * 255 + 15) / 31;
        uint8_t g = (g6 * 255 + 31) / 63;
        uint8_t b = (b5 * 255 + 15) / 31;
        uint8_t lum = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
        bool black = ctx->invert ? (lum >= 128) : (lum < 128);
        int dstX = ctx->x0 + dx;
        if (dstX >= 0 && dstX < DISPLAY_WIDTH && black) {
            lineBits[dstX >> 3] &= (uint8_t)~(0x80 >> (dstX & 7));
        }
    }

    // Vertical scaling: replicate or skip lines based on sY
    int yStart = ctx->y0 + (int)floorf(pDraw->y * ctx->sY);
    int yEnd   = ctx->y0 + (int)floorf((pDraw->y + 1) * ctx->sY) - 1;
    if (yEnd < yStart) yEnd = yStart;

    for (int dy = yStart; dy <= yEnd; ++dy) {
        if (dy < 0 || dy >= DISPLAY_HEIGHT) continue;
        epd.drawBitmap(0, dy, lineBits, DISPLAY_WIDTH, 1, GxEPD_BLACK);
    }

    return 1; // continue
}

// Simple command buffer to accumulate text draws until updateDisplay()
struct TextCmd { String text; int x; int y; int size; };
static TextCmd g_text_cmds[16];
static int g_text_cmd_count = 0;

PaperdInkHardware::PaperdInkHardware()
    : display(nullptr)
    , displayType(DISPLAY_BW)
    , batteryVoltage(0.0)
    , chargingStatus(false)
    , lowBattery(false)
    , sdCardAvailable(false) {

    // Initialize button states
    for (int i = 0; i < 4; i++) {
        buttonPressTime[i] = 0;
        buttonStates[i] = BUTTON_RELEASED;
        buttonPressed[i] = false;
    }
}

PaperdInkHardware::~PaperdInkHardware() {
    end();
}

bool PaperdInkHardware::begin() {
    #if DEBUG_ENABLED
    Serial.println("Initializing paperd.ink hardware...");
    #endif

    // Initialize pins
    initializePins();

    // Initialize preferences
    if (!preferences.begin("paperdink", false)) {
        #if DEBUG_ENABLED
        Serial.println("Failed to initialize preferences");
        #endif
        return false;
    }

    // Load persisted invert setting
    invertDisplayFlag = loadBool("invert", false);

    // Initialize display
    if (!initializeDisplay()) {
        #if DEBUG_ENABLED
        Serial.println("Failed to initialize display");
        #endif
        return false;
    }


    // Initialize SD card
    initializeSDCard();

    // Initialize buttons
    initializeButtons();

    // Read initial battery voltage
    batteryVoltage = readBatteryVoltage();
    chargingStatus = checkChargingStatus();

    #if DEBUG_ENABLED
    Serial.println("Hardware initialization complete");
    Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage, getBatteryPercentage());
    Serial.printf("SD Card: %s\n", sdCardAvailable ? "Available" : "Not Available");
    #endif

    return true;
}

void PaperdInkHardware::end() {
    // Note: Display functions are stubs for now
    #if DEBUG_ENABLED
    Serial.println("Hardware shutdown");
    #endif
    preferences.end();
    SD.end();
}

void PaperdInkHardware::initializePins() {
    // Initialize power control pins
    pinMode(EPD_ENABLE_PIN, OUTPUT);
    pinMode(SD_ENABLE_PIN, OUTPUT);
    pinMode(BATTERY_ENABLE_PIN, OUTPUT);

    // Enable peripherals initially
    digitalWrite(EPD_ENABLE_PIN, LOW);   // Active low
    digitalWrite(SD_ENABLE_PIN, LOW);    // Active low
    digitalWrite(BATTERY_ENABLE_PIN, HIGH);


    // Initialize button pins
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
    pinMode(BUTTON_3_PIN, INPUT_PULLUP);
    pinMode(BUTTON_4_PIN, INPUT_PULLUP);

    // Initialize buzzer pin
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // Initialize battery monitoring
    pinMode(BATTERY_VOLTAGE_PIN, INPUT);
    pinMode(CHARGING_INDICATOR_PIN, INPUT);

    // Initialize I2C
    Wire.begin(SDA_PIN, SCL_PIN);

    // Initialize SPI
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
}

bool PaperdInkHardware::initializeDisplay() {
    // Power on display
    digitalWrite(EPD_ENABLE_PIN, LOW);  // Active low
    delay(100);

    // Initialize display based on type
    displayType = DISPLAY_BW;   // Default to monochrome

    // Bring up the panel
    epd.init(0); // use default SPI frequency
    epd.setRotation(DISPLAY_ROTATION);
    epd.setTextColor(GxEPD_BLACK);
    epd.setFullWindow();
    epd.firstPage();
    do { epd.fillScreen(GxEPD_WHITE); } while (epd.nextPage());

    #if DEBUG_ENABLED
    Serial.println("Display initialization completed (GxEPD2 4.2\" B/W)");
    #endif

    return true;
}

void PaperdInkHardware::initializeSDCard() {
    // Enable SD card power
    digitalWrite(SD_ENABLE_PIN, LOW);  // Active low
    delay(100);


    // Initialize SD card
    if (SD.begin(SD_CS_PIN)) {
        sdCardAvailable = true;
        #if DEBUG_ENABLED
        Serial.println("SD card initialized successfully");
        Serial.printf("SD card size: %lluMB\n", SD.cardSize() / (1024 * 1024));
        #endif
    } else {
        sdCardAvailable = false;
        #if DEBUG_ENABLED
        Serial.println("SD card initialization failed");
        #endif
    }
}

// Display options setters/getters (out-of-line)
void PaperdInkHardware::setInvertDisplay(bool invert) {
    invertDisplayFlag = invert;
    saveBool("invert", invert);
}

bool PaperdInkHardware::getInvertDisplay() const {
    return invertDisplayFlag;
}

void PaperdInkHardware::initializeButtons() {
    // Buttons are already configured as INPUT_PULLUP in initializePins()
    // Initialize button states
    for (int i = 0; i < 4; i++) {
        buttonPressTime[i] = 0;
        buttonStates[i] = BUTTON_RELEASED;
        buttonPressed[i] = false;
    }

    #if DEBUG_ENABLED
    Serial.println("Buttons initialized");
    #endif
}

void PaperdInkHardware::updateButtons() {
    int buttonPins[4] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN, BUTTON_4_PIN};
    unsigned long currentTime = millis();

    for (int i = 0; i < 4; i++) {
        bool currentPressed = !digitalRead(buttonPins[i]);  // Active low with pullup

        if (currentPressed && !buttonPressed[i]) {
            // Button just pressed
            buttonPressed[i] = true;
            buttonPressTime[i] = currentTime;
            buttonStates[i] = BUTTON_PRESSED;
        } else if (!currentPressed && buttonPressed[i]) {
            // Button just released
            buttonPressed[i] = false;
            buttonStates[i] = BUTTON_RELEASED;
        } else if (currentPressed && buttonPressed[i]) {
            // Button held down - check for long press
            unsigned long pressedDuration = currentTime - buttonPressTime[i];

            if (pressedDuration > BUTTON_VERY_LONG_PRESS_MS) {
                buttonStates[i] = BUTTON_VERY_LONG_PRESS;
            } else if (pressedDuration > BUTTON_LONG_PRESS_MS) {
                buttonStates[i] = BUTTON_LONG_PRESS;
            }
        }
    }
}

float PaperdInkHardware::readBatteryVoltage() {
    // Enable battery voltage reading
    digitalWrite(BATTERY_ENABLE_PIN, HIGH);
    delay(10);

    // Read ADC value
    int adcValue = analogRead(BATTERY_VOLTAGE_PIN);

    // Disable battery voltage reading to save power
    digitalWrite(BATTERY_ENABLE_PIN, LOW);

    // Convert ADC to voltage (ESP32 ADC with voltage divider)
    // Assuming 2:1 voltage divider and 3.3V reference
    float voltage = (adcValue / 4095.0) * 3.3 * 2.0;

    #ifdef BATTERY_CALIBRATION_OFFSET
    voltage += BATTERY_CALIBRATION_OFFSET;
    #endif

    return voltage;
}

bool PaperdInkHardware::checkChargingStatus() {
    // Read charging indicator pin
    return digitalRead(CHARGING_INDICATOR_PIN) == LOW;  // Active low
}

// Display methods (stub implementations)
void PaperdInkHardware::clearDisplay() {
    epd.firstPage();
    do { epd.fillScreen(GxEPD_WHITE); } while (epd.nextPage());
}

void PaperdInkHardware::updateDisplay() {
    epd.firstPage();
    do {
        epd.fillScreen(GxEPD_WHITE);
        for (int i = 0; i < g_text_cmd_count; ++i) {
            epd.setCursor(g_text_cmds[i].x, g_text_cmds[i].y);
            epd.setTextSize(g_text_cmds[i].size);
            epd.print(g_text_cmds[i].text);
        }
    } while (epd.nextPage());
    g_text_cmd_count = 0; // clear buffer
}

void PaperdInkHardware::partialUpdateDisplay() {
    // For simplicity, re-use full update
    updateDisplay();
}

void PaperdInkHardware::displayText(const char* text, int x, int y, int size) {
    if (g_text_cmd_count < (int)(sizeof(g_text_cmds)/sizeof(g_text_cmds[0]))) {
        g_text_cmds[g_text_cmd_count++] = TextCmd{String(text), x, y, size};
    }
}

void PaperdInkHardware::displayImage(const uint8_t* imageData, size_t imageSize) {
    if (!imageData || imageSize == 0) return;

    #if DEBUG_ENABLED
    Serial.printf("Displaying image of size: %d bytes\n", imageSize);
    #endif

    // Try to detect a simple 1-bit raw buffer (exact display size)
    const size_t raw1bppSize = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8;
    if (imageSize == raw1bppSize) {
        // Draw raw 1-bit bitmap
        epd.setFullWindow();
        epd.firstPage();
        do {
            epd.fillScreen(GxEPD_WHITE);
            // GxEPD2 expects 1-bit bitmap MSB first; assume incoming buffer is MSB-first
            epd.drawBitmap(0, 0, imageData, DISPLAY_WIDTH, DISPLAY_HEIGHT, GxEPD_BLACK);
        } while (epd.nextPage());
        return;
    }

    // Otherwise assume PNG (1-bit or grayscale) and decode with PNGdec
    int rc = s_png.openRAM((uint8_t*)imageData, (int)imageSize, pngDrawToEPD);
    if (rc != PNG_SUCCESS) {
        #if DEBUG_ENABLED
        Serial.println("PNG openRAM failed");
        #endif
        // fallback: clear
        clearDisplay();
        displayText("PNG decode failed", 10, 60, 1);
        updateDisplay();
        return;
    }

    // Read PNG size
    int16_t pngW = s_png.getWidth();
    int16_t pngH = s_png.getHeight();
    #if DEBUG_ENABLED
    Serial.printf("PNG size: %dx%d\n", pngW, pngH);
    #endif

    // Compute uniform scale to fit into DISPLAY (letterbox/pillarbox), keep aspect ratio
    float s = 1.0f;
    if (pngW > 0 && pngH > 0) {
        float sx = (float)DISPLAY_WIDTH / (float)pngW;
        float sy = (float)DISPLAY_HEIGHT / (float)pngH;
        s = sx < sy ? sx : sy;
        if (s <= 0.0f) s = 1.0f;
    }
    int tW = (int)floorf(pngW * s);
    int tH = (int)floorf(pngH * s);
    if (tW < 1) tW = 1;
    if (tH < 1) tH = 1;

    // Centered placement
    PngDrawContext ctx;
    ctx.tW = tW;
    ctx.tH = tH;
    ctx.sX = s;
    ctx.sY = s;
    ctx.x0 = (DISPLAY_WIDTH - tW) / 2;
    ctx.y0 = (DISPLAY_HEIGHT - tH) / 2;
    ctx.invert = invertDisplayFlag;

    // Close after reading header; reopen per page (required for GxEPD2 paging)
    s_png.close();

    // Refresh invert per page in case it changed
    ctx.invert = invertDisplayFlag;

    epd.setFullWindow();
    epd.firstPage();
    do {
        epd.fillScreen(GxEPD_WHITE);
        int orc = s_png.openRAM((uint8_t*)imageData, (int)imageSize, pngDrawToEPD);
        if (orc != PNG_SUCCESS) {
            #if DEBUG_ENABLED
            Serial.println("PNG openRAM failed on page");
            #endif
            break;
        }
        int dec = s_png.decode(&ctx, 0);
        s_png.close();
        if (dec != PNG_SUCCESS) {
            #if DEBUG_ENABLED
            Serial.printf("PNG decode error: %d\n", dec);
            #endif
            break;
        }
    } while (epd.nextPage());
}

// Button methods
ButtonState PaperdInkHardware::getButtonState(int buttonNum) {
    if (buttonNum < 0 || buttonNum >= 4) return BUTTON_RELEASED;
    return buttonStates[buttonNum];
}

bool PaperdInkHardware::isButtonPressed(int buttonNum) {
    if (buttonNum < 0 || buttonNum >= 4) return false;
    return buttonStates[buttonNum] == BUTTON_PRESSED;
}

bool PaperdInkHardware::isButtonLongPressed(int buttonNum) {
    if (buttonNum < 0 || buttonNum >= 4) return false;
    return buttonStates[buttonNum] == BUTTON_LONG_PRESS;
}

bool PaperdInkHardware::isButtonVeryLongPressed(int buttonNum) {
    if (buttonNum < 0 || buttonNum >= 4) return false;
    return buttonStates[buttonNum] == BUTTON_VERY_LONG_PRESS;
}

void PaperdInkHardware::resetButtonState(int buttonNum) {
    if (buttonNum < 0 || buttonNum >= 4) return;
    buttonStates[buttonNum] = BUTTON_RELEASED;
    buttonPressed[buttonNum] = false;
    buttonPressTime[buttonNum] = 0;
}

// Power management
float PaperdInkHardware::getBatteryVoltage() {
    batteryVoltage = readBatteryVoltage();
    return batteryVoltage;
}

int PaperdInkHardware::getBatteryPercentage() {
    float voltage = getBatteryVoltage();

    // LiPo voltage curve approximation
    if (voltage >= 4.1) return 100;
    if (voltage >= 4.0) return 90;
    if (voltage >= 3.9) return 80;
    if (voltage >= 3.8) return 70;
    if (voltage >= 3.7) return 60;
    if (voltage >= 3.6) return 50;
    if (voltage >= 3.5) return 40;
    if (voltage >= 3.4) return 30;
    if (voltage >= 3.3) return 20;
    if (voltage >= 3.2) return 10;
    return 0;
}

bool PaperdInkHardware::isLowBattery() {
    return getBatteryVoltage() < LOW_BATTERY_THRESHOLD;
}

bool PaperdInkHardware::isCriticalBattery() {
    return getBatteryVoltage() < CRITICAL_BATTERY_THRESHOLD;
}

bool PaperdInkHardware::isCharging() {
    chargingStatus = checkChargingStatus();
    return chargingStatus;
}

void PaperdInkHardware::enterDeepSleep(uint32_t sleepTimeSeconds) {
    #if DEBUG_ENABLED
    Serial.printf("Entering deep sleep for %d seconds\n", sleepTimeSeconds);
    #endif

    // Configure wakeup sources
    esp_sleep_enable_timer_wakeup(sleepTimeSeconds * 1000000ULL);  // Convert to microseconds

    // Configure button wakeup (any button)
    esp_sleep_enable_ext1_wakeup(
        (1ULL << BUTTON_1_PIN) | (1ULL << BUTTON_2_PIN) |
        (1ULL << BUTTON_3_PIN) | (1ULL << BUTTON_4_PIN),
        ESP_EXT1_WAKEUP_ANY_HIGH
    );

    // Power down peripherals
    disablePeripherals();

    // Enter deep sleep
    esp_deep_sleep_start();
}

void PaperdInkHardware::disablePeripherals() {
    // Power down display (stub)
    digitalWrite(EPD_ENABLE_PIN, HIGH);  // Disable display power

    // Power down SD card
    SD.end();
    digitalWrite(SD_ENABLE_PIN, HIGH);  // Disable SD card power

    // Disable WiFi and Bluetooth
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_deinit();
    esp_bt_controller_disable();
}

void PaperdInkHardware::enablePeripherals() {
    // Enable display power
    digitalWrite(EPD_ENABLE_PIN, LOW);  // Active low
    delay(100);

    // Enable SD card power
    digitalWrite(SD_ENABLE_PIN, LOW);  // Active low
    delay(100);

    // Reinitialize SD card if it was available before
    if (sdCardAvailable) {
        SD.begin(SD_CS_PIN);
    }
}

// SD Card methods
bool PaperdInkHardware::isSDCardAvailable() {
    return sdCardAvailable;
}

bool PaperdInkHardware::writeFile(const char* path, const uint8_t* data, size_t size) {
    if (!sdCardAvailable) return false;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        #if DEBUG_ENABLED
        Serial.printf("Failed to open file for writing: %s\n", path);
        #endif
        return false;
    }

    size_t written = file.write(data, size);
    file.close();

    return written == size;
}

bool PaperdInkHardware::readFile(const char* path, uint8_t* buffer, size_t maxSize, size_t* actualSize) {
    if (!sdCardAvailable) return false;

    File file = SD.open(path, FILE_READ);
    if (!file) {
        #if DEBUG_ENABLED
        Serial.printf("Failed to open file for reading: %s\n", path);
        #endif
        return false;
    }

    size_t fileSize = file.size();
    size_t readSize = min(fileSize, maxSize);

    size_t bytesRead = file.read(buffer, readSize);
    file.close();

    if (actualSize) {
        *actualSize = bytesRead;
    }

    return bytesRead > 0;
}

bool PaperdInkHardware::fileExists(const char* path) {
    if (!sdCardAvailable) return false;
    return SD.exists(path);
}

size_t PaperdInkHardware::getFileSize(const char* path) {
    if (!sdCardAvailable) return 0;

    File file = SD.open(path, FILE_READ);
    if (!file) return 0;

    size_t size = file.size();
    file.close();
    return size;
}

bool PaperdInkHardware::deleteFile(const char* path) {
    if (!sdCardAvailable) return false;
    return SD.remove(path);
}

bool PaperdInkHardware::formatSDCard() {
    if (!sdCardAvailable) return false;

    // Recursively delete all files and directories on the SD card
    std::function<bool(const char*)> rmrf = [&](const char* path) -> bool {
        File entry = SD.open(path);
        if (!entry) return false;
        if (!entry.isDirectory()) {
            entry.close();
            return SD.remove(path);
        }
        // Directory
        File child;
        bool ok = true;
        while ((child = entry.openNextFile())) {
            String childPath = String(path);
            if (!child.isDirectory()) {
                ok = ok && SD.remove((childPath + "/" + child.name()).c_str());
            } else {
                String subdir = childPath + "/" + child.name();
                child.close();
                ok = ok && rmrf(subdir.c_str());
            }
        }
        entry.close();
        // Remove the directory itself if not root
        if (String(path) != "/") {
            ok = ok && SD.rmdir(path);
        }
        return ok;
    };

    bool result = rmrf("/");
    #if DEBUG_ENABLED
    Serial.printf("SD format (rm -rf) result: %s\n", result ? "OK" : "FAIL");
    #endif
    return result;
}


// Buzzer methods
void PaperdInkHardware::beep(int frequency, int duration) {
    tone(BUZZER_PIN, frequency, duration);
    delay(duration);
    noTone(BUZZER_PIN);
}

void PaperdInkHardware::playTone(int frequency, int duration) {
    tone(BUZZER_PIN, frequency, duration);
}

// Preferences methods
bool PaperdInkHardware::saveString(const char* key, const char* value) {
    return preferences.putString(key, value) > 0;
}

String PaperdInkHardware::loadString(const char* key, const char* defaultValue) {
    return preferences.getString(key, defaultValue);
}

bool PaperdInkHardware::saveInt(const char* key, int value) {
    return preferences.putInt(key, value) > 0;
}

int PaperdInkHardware::loadInt(const char* key, int defaultValue) {
    return preferences.getInt(key, defaultValue);
}

bool PaperdInkHardware::saveBool(const char* key, bool value) {
    return preferences.putBool(key, value) > 0;
}

bool PaperdInkHardware::loadBool(const char* key, bool defaultValue) {
    return preferences.getBool(key, defaultValue);
}

void PaperdInkHardware::clearPreferences() {
    preferences.clear();
}

// Utility methods
String PaperdInkHardware::getMacAddress() {
    // Try to get base MAC via esp_read_mac if WiFi not yet initialized
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
    // Fallback to esp_wifi_get_mac (requires WiFi init) and then to WiFi.macAddress()
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return String(buf);
    }
    return WiFi.macAddress();
}

void PaperdInkHardware::restart() {
    ESP.restart();
}

void PaperdInkHardware::factoryReset() {
    clearPreferences();
    if (sdCardAvailable) {
        // Clear cache files
        deleteFile("/cache");
    }
    delay(1000);
    restart();
}

uint32_t PaperdInkHardware::getFreeHeap() {
    return ESP.getFreeHeap();
}

void PaperdInkHardware::printSystemInfo() {
    #if DEBUG_ENABLED
    Serial.println("=== System Information ===");
    Serial.printf("Firmware: %s\n", FIRMWARE_VERSION);
    Serial.printf("MAC Address: %s\n", getMacAddress().c_str());
    Serial.printf("Free Heap: %d bytes\n", getFreeHeap());
    Serial.printf("Battery: %.2fV (%d%%)\n", getBatteryVoltage(), getBatteryPercentage());
    Serial.printf("Charging: %s\n", isCharging() ? "Yes" : "No");
    Serial.printf("SD Card: %s\n", isSDCardAvailable() ? "Available" : "Not Available");
    Serial.println("========================");
    #endif
}

// Missing display functions
bool PaperdInkHardware::initDisplay(DisplayType type) {
    displayType = type;
    return initializeDisplay();
}

void PaperdInkHardware::displayBitmap(const uint8_t* bitmap, int x, int y, int w, int h) {
    #if DEBUG_ENABLED
    Serial.printf("Display bitmap (stub): %dx%d at (%d,%d)\n", w, h, x, y);
    #endif
}

void PaperdInkHardware::setRotation(int rotation) {
    epd.setRotation(rotation);
}

void PaperdInkHardware::powerOffDisplay() {
    epd.hibernate();
}

void PaperdInkHardware::powerOnDisplay() {
    epd.init(0);
    epd.setRotation(DISPLAY_ROTATION);
}

void PaperdInkHardware::enterLightSleep(uint32_t sleepTimeMs) {
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000ULL);  // Convert to microseconds
    esp_light_sleep_start();
}