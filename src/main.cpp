/**
 * paperd.ink TRMNL Firmware
 *
 * A TRMNL-compatible firmware for the paperd.ink e-paper device
 * Features:
 * - 4-button navigation
 * - SD card caching
 * - Tri-color e-paper display support
 * - Extended power management
 * - Offline mode capability
 *
 * Author: AI Assistant
 * Version: 1.0.0
 */

#include <Arduino.h>
#include "config.h"
#include "paperdink_hardware.h"
#include "trmnl_client.h"

// Global objects
PaperdInkHardware hardware;
TRMNLClient trmnlClient(&hardware);

// State variables
unsigned long lastUpdateTime = 0;
unsigned long lastButtonCheck = 0;
bool systemInitialized = false;
bool forceRefresh = false;

// Function prototypes
void setup();
void loop();
void handleButtons();
void handleSystemStates();
void performStartupSequence();
void showStartupScreen();
void showErrorScreen(const String& error);
void showStatusScreen();
void enterSleepMode();
bool checkWakeupReason();
void handleFactoryReset();

void setup() {
    // Initialize serial communication for debugging
    #if DEBUG_ENABLED
    Serial.begin(DEBUG_SERIAL_SPEED);
    while (!Serial && millis() < 3000) {
        delay(10);
    }
    Serial.println("=== paperd.ink TRMNL Firmware Starting ===");
    Serial.println("Version: " FIRMWARE_VERSION);
    #endif

    // Check wakeup reason
    bool userWakeup = checkWakeupReason();

    // Initialize hardware
    if (!hardware.begin()) {
        #if DEBUG_ENABLED
        Serial.println("ERROR: Hardware initialization failed!");
        #endif
        showErrorScreen("Hardware Init Failed");
        delay(5000);
        ESP.restart();
        return;
    }

    // Show startup screen
    showStartupScreen();

    // Check for factory reset (Button 4 held during startup)
    if (hardware.isButtonVeryLongPressed(3)) {  // Button 4 (index 3)
        handleFactoryReset();
        return;
    }

    // Initialize TRMNL client
    if (!trmnlClient.begin()) {
        #if DEBUG_ENABLED
        Serial.println("ERROR: TRMNL client initialization failed!");
        #endif
        showErrorScreen("TRMNL Init Failed");
        delay(5000);
    }

    // Perform startup sequence
    performStartupSequence();

    systemInitialized = true;
    lastUpdateTime = millis();

    #if DEBUG_ENABLED
    Serial.println("=== System initialized successfully ===");
    hardware.printSystemInfo();
    trmnlClient.printStatus();
    #endif
}

void loop() {
    if (!systemInitialized) {
        delay(1000);
        return;
    }

    // Update hardware states
    hardware.updateButtons();

    // Handle button inputs
    handleButtons();

    // Handle TRMNL client operations
    trmnlClient.loop();

    // Handle system states
    handleSystemStates();

    // Check if it's time for a content update
    unsigned long currentTime = millis();
    if (forceRefresh ||
        (currentTime - lastUpdateTime > (trmnlClient.getRefreshRate() * 1000))) {

        if (trmnlClient.updateContent()) {
            lastUpdateTime = currentTime;
            forceRefresh = false;

            #if DEBUG_ENABLED
            Serial.println("Content updated successfully");
            #endif
        } else {
            #if DEBUG_ENABLED
            Serial.println("Content update failed: " + trmnlClient.getLastError());
            #endif

            // Try offline mode if available
            if (trmnlClient.hasCachedContent()) {
                trmnlClient.displayCachedContent();
            }
        }
    }

    // Check battery level and enter sleep if needed
    if (hardware.isCriticalBattery()) {
        showErrorScreen("Critical Battery");
        delay(2000);
        enterSleepMode();
    }

    // Small delay to prevent excessive CPU usage
    delay(100);
}

void handleButtons() {
    static unsigned long lastButtonTime = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastButtonTime < BUTTON_DEBOUNCE_MS) {
        return;
    }

    // Button 1: Manual refresh / Wake
    if (hardware.getButtonState(0) == BUTTON_PRESSED) {
        #if DEBUG_ENABLED
        Serial.println("Button 1 pressed: Manual refresh");
        #endif
        forceRefresh = true;
        hardware.resetButtonState(0);
        lastButtonTime = currentTime;
    }

    // Button 2: Cycle through playlists/screens (future feature)
    if (hardware.getButtonState(1) == BUTTON_PRESSED) {
        #if DEBUG_ENABLED
        Serial.println("Button 2 pressed: Cycle screens");
        #endif
        // TODO: Implement playlist cycling
        hardware.beep(800, 100);
        hardware.resetButtonState(1);
        lastButtonTime = currentTime;
    }

    // Button 3: Settings/Configuration mode
    if (hardware.getButtonState(2) == BUTTON_LONG_PRESS) {
        #if DEBUG_ENABLED
        Serial.println("Button 3 long press: Configuration mode");
        #endif
        showStatusScreen();
        hardware.resetButtonState(2);
        lastButtonTime = currentTime;
    }

    // Button 4: Power/Sleep toggle
    if (hardware.getButtonState(3) == BUTTON_PRESSED) {
        #if DEBUG_ENABLED
        Serial.println("Button 4 pressed: Sleep mode");
        #endif
        enterSleepMode();
        hardware.resetButtonState(3);
        lastButtonTime = currentTime;
    }

    // Button 4 very long press: Factory reset (handled in setup)
    if (hardware.getButtonState(3) == BUTTON_VERY_LONG_PRESS) {
        handleFactoryReset();
    }
}

void handleSystemStates() {
    DeviceState currentState = trmnlClient.getState();

    switch (currentState) {
        case STATE_WIFI_SETUP:
            // Handle WiFi configuration portal
            if (!trmnlClient.hasWiFiCredentials()) {
                if (!trmnlClient.startConfigPortal()) {
                    showErrorScreen("WiFi Setup Failed");
                    delay(5000);
                    enterSleepMode();
                }
            }
            break;

        case STATE_DEVICE_SETUP:
            // Handle device registration
            if (!trmnlClient.registerDevice()) {
                showErrorScreen("Device Registration Failed");
                delay(5000);
                enterSleepMode();
            }
            break;

        case STATE_ERROR:
            // Handle error state
            if (trmnlClient.getConsecutiveErrors() > 5) {
                showErrorScreen("Too Many Errors");
                delay(10000);
                trmnlClient.clearErrors();
                enterSleepMode();
            }
            break;

        case STATE_OFFLINE:
            // Try to reconnect periodically
            static unsigned long lastReconnectAttempt = 0;
            if (millis() - lastReconnectAttempt > 300000) {  // 5 minutes
                if (trmnlClient.isWiFiConnected()) {
                    trmnlClient.setState(STATE_OPERATIONAL);
                }
                lastReconnectAttempt = millis();
            }
            break;

        default:
            break;
    }
}

void performStartupSequence() {
    #if DEBUG_ENABLED
    Serial.println("Starting startup sequence...");
    #endif

    // Check WiFi credentials
    if (!trmnlClient.hasWiFiCredentials()) {
        trmnlClient.setState(STATE_WIFI_SETUP);
        hardware.displayText("WiFi Setup Required", 10, 100, 2);
        hardware.displayText("Connect to paperdink-setup", 10, 130, 1);
        hardware.displayText("to configure WiFi", 10, 150, 1);
        hardware.updateDisplay();
        return;
    }

    // Check device registration
    if (!trmnlClient.isDeviceRegistered()) {
        trmnlClient.setState(STATE_DEVICE_SETUP);
        hardware.displayText("Device Setup", 10, 100, 2);
        hardware.displayText("Registering with TRMNL...", 10, 130, 1);
        hardware.updateDisplay();
        return;
    }

    // All good, set operational state
    trmnlClient.setState(STATE_OPERATIONAL);
    hardware.displayText("Ready", 10, 100, 3);
    hardware.displayText("paperd.ink TRMNL", 10, 140, 2);
    hardware.updateDisplay();
    delay(2000);
}

void showStartupScreen() {
    hardware.clearDisplay();
    hardware.displayText("paperd.ink", 50, 80, 3);
    hardware.displayText("TRMNL Edition", 50, 120, 2);
    hardware.displayText("v" FIRMWARE_VERSION, 50, 150, 1);

    // Show battery level
    float batteryVoltage = hardware.getBatteryVoltage();
    int batteryPercent = hardware.getBatteryPercentage();
    String batteryText = "Battery: " + String(batteryPercent) + "% (" + String(batteryVoltage, 2) + "V)";
    hardware.displayText(batteryText.c_str(), 10, 200, 1);

    // Show MAC address
    String macText = "MAC: " + hardware.getMacAddress();
    hardware.displayText(macText.c_str(), 10, 220, 1);

    hardware.updateDisplay();
    delay(3000);
}

void showErrorScreen(const String& error) {
    hardware.clearDisplay();
    hardware.displayText("ERROR", 50, 80, 3);
    hardware.displayText(error.c_str(), 10, 120, 2);
    hardware.displayText("Press any button", 10, 160, 1);
    hardware.displayText("to continue", 10, 180, 1);
    hardware.updateDisplay();

    // Beep to indicate error
    hardware.beep(400, 200);
    delay(200);
    hardware.beep(400, 200);
}

void showStatusScreen() {
    hardware.clearDisplay();
    hardware.displayText("Status", 10, 20, 2);

    // WiFi status
    String wifiStatus = "WiFi: ";
    if (trmnlClient.isWiFiConnected()) {
        wifiStatus += trmnlClient.getWiFiSSID() + " (" + String(trmnlClient.getWiFiRSSI()) + "dBm)";
    } else {
        wifiStatus += "Disconnected";
    }
    hardware.displayText(wifiStatus.c_str(), 10, 50, 1);

    // Device status
    String deviceStatus = "Device: " + trmnlClient.getFriendlyId();
    hardware.displayText(deviceStatus.c_str(), 10, 70, 1);

    // Battery status
    String batteryStatus = "Battery: " + String(hardware.getBatteryPercentage()) + "%";
    if (hardware.isCharging()) {
        batteryStatus += " (Charging)";
    }
    hardware.displayText(batteryStatus.c_str(), 10, 90, 1);

    // SD Card status
    String sdStatus = "SD Card: ";
    sdStatus += hardware.isSDCardAvailable() ? "Available" : "Not Available";
    hardware.displayText(sdStatus.c_str(), 10, 110, 1);

    // Last update
    String updateStatus = "Last Update: " + String((millis() - lastUpdateTime) / 1000) + "s ago";
    hardware.displayText(updateStatus.c_str(), 10, 130, 1);

    // Refresh rate
    String refreshStatus = "Refresh: " + String(trmnlClient.getRefreshRate()) + "s";
    hardware.displayText(refreshStatus.c_str(), 10, 150, 1);

    hardware.displayText("Press Button 3 again to exit", 10, 200, 1);
    hardware.updateDisplay();

    // Wait for button press to exit
    while (!hardware.isButtonPressed(2)) {
        hardware.updateButtons();
        delay(100);
    }
    hardware.resetButtonState(2);
}

void enterSleepMode() {
    #if DEBUG_ENABLED
    Serial.println("Entering sleep mode...");
    #endif

    // Show sleep screen
    hardware.clearDisplay();
    hardware.displayText("Sleeping...", 50, 100, 2);
    hardware.displayText("Press any button", 10, 140, 1);
    hardware.displayText("to wake up", 10, 160, 1);
    hardware.updateDisplay();

    // Disable peripherals to save power
    hardware.disablePeripherals();

    // Calculate sleep duration
    uint32_t sleepDuration = trmnlClient.getRefreshRate();
    if (sleepDuration < 300) sleepDuration = 300;  // Minimum 5 minutes

    // Enter deep sleep
    hardware.enterDeepSleep(sleepDuration);
}

bool checkWakeupReason() {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    switch (wakeupReason) {
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
            #if DEBUG_ENABLED
            Serial.println("Wakeup caused by external signal (button)");
            #endif
            return true;

        case ESP_SLEEP_WAKEUP_TIMER:
            #if DEBUG_ENABLED
            Serial.println("Wakeup caused by timer");
            #endif
            return false;

        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            #if DEBUG_ENABLED
            Serial.println("Wakeup caused by touchpad");
            #endif
            return true;

        case ESP_SLEEP_WAKEUP_ULP:
            #if DEBUG_ENABLED
            Serial.println("Wakeup caused by ULP program");
            #endif
            return false;

        default:
            #if DEBUG_ENABLED
            Serial.println("Wakeup was not caused by deep sleep (first boot or reset)");
            #endif
            return true;
    }
}

void handleFactoryReset() {
    #if DEBUG_ENABLED
    Serial.println("Factory reset requested!");
    #endif

    hardware.clearDisplay();
    hardware.displayText("FACTORY RESET", 10, 80, 2);
    hardware.displayText("Clearing all data...", 10, 120, 1);
    hardware.updateDisplay();

    // Clear all stored data
    trmnlClient.clearWiFiCredentials();
    trmnlClient.clearDeviceRegistration();
    hardware.clearPreferences();

    // Clear SD card cache if available
    if (hardware.isSDCardAvailable()) {
        // TODO: Clear cache files
    }

    hardware.displayText("Reset complete!", 10, 160, 1);
    hardware.displayText("Restarting...", 10, 180, 1);
    hardware.updateDisplay();

    // Beep to confirm reset
    for (int i = 0; i < 3; i++) {
        hardware.beep(1000, 200);
        delay(300);
    }

    delay(2000);
    hardware.restart();
}