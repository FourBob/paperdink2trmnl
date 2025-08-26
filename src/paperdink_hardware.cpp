#include "paperdink_hardware.h"
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <WiFi.h>
#include <esp_system.h>

// Display driver includes - using generic GxEPD2 for compatibility
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

// Note: This is a placeholder display instance
// In a real implementation, you would select the correct display driver
// based on your specific paperd.ink model (e.g., GxEPD2_420, GxEPD2_290, etc.)
// For now, we'll use a null pointer and implement display functions as stubs
GxEPD2_GFX* displayInstance = nullptr;

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
    display = displayInstance;  // Will be nullptr for now
    displayType = DISPLAY_BW;   // Default to monochrome

    // Note: In a real implementation, you would initialize the specific
    // display driver for your paperd.ink model here
    // For example:
    // display = new GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>(
    //     GxEPD2_420(EPD_CS_PIN, EPD_DC_PIN, EPD_RESET_PIN, EPD_BUSY_PIN));
    // display->init(115200);

    #if DEBUG_ENABLED
    Serial.println("Display initialization completed (stub implementation)");
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
    #if DEBUG_ENABLED
    Serial.println("Clear display (stub)");
    #endif
}

void PaperdInkHardware::updateDisplay() {
    #if DEBUG_ENABLED
    Serial.println("Update display (stub)");
    #endif
}

void PaperdInkHardware::partialUpdateDisplay() {
    #if DEBUG_ENABLED
    Serial.println("Partial update display (stub)");
    #endif
}

void PaperdInkHardware::displayText(const char* text, int x, int y, int size) {
    if (!display) {
        #if DEBUG_ENABLED
        Serial.printf("Display text (stub): '%s' at (%d,%d) size %d\n", text, x, y, size);
        #endif
        return;
    }

    // Note: In a real implementation, you would set fonts and display text
    // For now, this is a stub implementation
    #if DEBUG_ENABLED
    Serial.printf("Display text: '%s' at (%d,%d) size %d\n", text, x, y, size);
    #endif
}

void PaperdInkHardware::displayImage(const uint8_t* imageData, size_t imageSize) {
    if (!display || !imageData) return;

    // This is a simplified implementation
    // In a real implementation, you would decode the image format
    // and convert it to the display's native format

    #if DEBUG_ENABLED
    Serial.printf("Displaying image of size: %d bytes\n", imageSize);
    #endif

    // For now, just clear and show a placeholder
    clearDisplay();
    displayText("Image Display", 10, 50, 2);
    displayText("Not Implemented", 10, 80, 1);
    updateDisplay();
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
    #if DEBUG_ENABLED
    Serial.printf("Set rotation (stub): %d\n", rotation);
    #endif
}

void PaperdInkHardware::powerOffDisplay() {
    digitalWrite(EPD_ENABLE_PIN, HIGH);  // Disable display power
    #if DEBUG_ENABLED
    Serial.println("Power off display");
    #endif
}

void PaperdInkHardware::powerOnDisplay() {
    digitalWrite(EPD_ENABLE_PIN, LOW);  // Enable display power
    delay(100);
    #if DEBUG_ENABLED
    Serial.println("Power on display");
    #endif
}

void PaperdInkHardware::enterLightSleep(uint32_t sleepTimeMs) {
    esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000ULL);  // Convert to microseconds
    esp_light_sleep_start();
}