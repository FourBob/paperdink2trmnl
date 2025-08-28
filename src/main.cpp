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
#include "secrets.h"

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
    Serial.begin(115200);
    delay(3000);  // Longer delay for stable boot

    // Ensure we're fully booted
    Serial.println();
    Serial.println("*** BOOT START ***");
    Serial.flush();
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("=== paperd.ink TRMNL Firmware v1.0 ===");
    Serial.println("========================================");
    Serial.println();

    Serial.print("ESP32 Chip ID: ");
    Serial.println(ESP.getChipModel());
    Serial.print("MAC Address: ");
    Serial.println(hardware.getMacAddress());
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.println();

    Serial.println("Configuration:");
    Serial.println("- DEBUG_ENABLED: " + String(DEBUG_ENABLED));
    Serial.println("- DEVELOPMENT_MODE: " + String(DEVELOPMENT_MODE));
    Serial.println("- WiFi SSID: " + String(WIFI_SSID));
    #ifdef CUSTOM_FRIENDLY_ID
    Serial.println("- Device ID: " + String(CUSTOM_FRIENDLY_ID));
    #else
    Serial.println("- Device ID: (not set)");
    #endif
    #ifdef CUSTOM_API_KEY
    Serial.println("- API Key: " + String(CUSTOM_API_KEY).substring(0, 8) + "...");
    #else
    Serial.println("- API Key: (not set)");
    #endif
    Serial.println();

    Serial.println("Starting hardware initialization...");

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
    // Trigger an immediate first content refresh after startup
    forceRefresh = true;
    lastUpdateTime = millis();

    #if DEBUG_ENABLED
    Serial.println("=== System initialized successfully ===");
    hardware.printSystemInfo();
    trmnlClient.printStatus();
    #endif
}

void loop() {
    #if DEBUG_ENABLED
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 2000) {  // Every 2 seconds
        Serial.printf("*** LOOP: systemInitialized=%s, State=%d ***\n",
                     systemInitialized ? "true" : "false",
                     trmnlClient.getState());
        lastDebug = millis();
    }
    #endif


    // Debug serial command to trigger Factory Reset: send "FR" or "FACTORY_RESET" over serial
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("FR") || cmd.equalsIgnoreCase("FACTORY_RESET")) {
            Serial.println("Serial command received: FACTORY RESET");
            handleFactoryReset();
            return; // handleFactoryReset will restart the device
        }
    }

    if (!systemInitialized) {
        #if DEBUG_ENABLED
        Serial.println("Loop: systemInitialized is false, returning");
        #endif
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
        (currentTime - lastUpdateTime >= (unsigned long)trmnlClient.getRefreshRate() * 1000UL)) {

        if (trmnlClient.updateContent()) {
            lastUpdateTime = currentTime;
            forceRefresh = false;

            #if DEBUG_ENABLED
            Serial.println("Content updated successfully");
            #endif

            // Nach erfolgreichem Update sofort schlafen, Wake per Timer/Button
            enterSleepMode();
            return;
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

    // Button 2: Toggle invert display
    if (hardware.getButtonState(1) == BUTTON_PRESSED) {
        bool inv = !hardware.getInvertDisplay();
        hardware.setInvertDisplay(inv);
        #if DEBUG_ENABLED
        Serial.printf("Button 2 pressed: Invert %s\n", inv ? "ON" : "OFF");
        #endif
        hardware.beep(inv ? 1000 : 600, 80);
        forceRefresh = true; // redraw current content with new invert mode
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

    #if DEBUG_ENABLED
    static unsigned long lastStateDebug = 0;
    if (millis() - lastStateDebug > 5000) {  // Every 5 seconds
        Serial.printf("*** handleSystemStates: State=%d ***\n", currentState);
        lastStateDebug = millis();
    }
    #endif

    switch (currentState) {
        case STATE_WIFI_SETUP:
            #if DEBUG_ENABLED
            Serial.println("Handling STATE_WIFI_SETUP");
            #endif
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
            #if DEBUG_ENABLED
            Serial.println("Handling STATE_DEVICE_SETUP - calling registerDevice()");
            #endif
            // Handle device registration
            if (!trmnlClient.registerDevice()) {
                #if DEBUG_ENABLED
                Serial.println("registerDevice() failed!");
                #endif
                showErrorScreen("Device Registration Failed");
                delay(5000);
                enterSleepMode();
            } else {
                #if DEBUG_ENABLED
                Serial.println("registerDevice() succeeded!");
                #endif
            }
            break;

        case STATE_OPERATIONAL:
            #if DEBUG_ENABLED
            Serial.println("Handling STATE_OPERATIONAL - checking WiFi and updating display");
            #endif
            // Handle operational state - check WiFi and update display
            if (!trmnlClient.isWiFiConnected()) {
                #if DEBUG_ENABLED
                Serial.println("WiFi disconnected in operational state - attempting reconnect");
                #endif
                // Try to register device again (which includes WiFi connection)
                if (!trmnlClient.registerDevice()) {
                    #if DEBUG_ENABLED
                    Serial.println("Device registration/WiFi reconnect failed - will retry later, staying operational");
                    #endif
                    // Stay in operational; we'll retry on the next loop
                }
            } else {
                // WiFi is connected, check for updates periodically
                static unsigned long lastUpdateCheck = 0;
                if (millis() - lastUpdateCheck > 60000) {  // Check every minute
                    #if DEBUG_ENABLED
                    Serial.println("Checking for content updates...");
                    #endif
                    if (trmnlClient.hasNewContent()) {
                        trmnlClient.updateContent();
                        trmnlClient.displayContent();
                    }
                    lastUpdateCheck = millis();
                }
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
        wifiStatus += trmnlClient.getWiFiSSID();
    } else {
        wifiStatus += "Disconnected";
    }
    hardware.displayText(wifiStatus.c_str(), 10, 50, 1);

    // Network details
    if (trmnlClient.isWiFiConnected()) {
        String ipLine = String("IP: ") + WiFi.localIP().toString();
        hardware.displayText(ipLine.c_str(), 10, 65, 1);
        String gwLine = String("GW: ") + WiFi.gatewayIP().toString();
        hardware.displayText(gwLine.c_str(), 10, 80, 1);
        String dnsLine = String("DNS: ") + WiFi.dnsIP().toString();
        hardware.displayText(dnsLine.c_str(), 10, 95, 1);
        String rssiLine = String("Signal: ") + String(trmnlClient.getWiFiRSSI()) + " dBm";
        hardware.displayText(rssiLine.c_str(), 10, 110, 1);
    }

    // Device identifiers
    String macLine = String("MAC: ") + hardware.getMacAddress();
    hardware.displayText(macLine.c_str(), 10, 125, 1);
    String deviceStatus = String("Device: ") + trmnlClient.getFriendlyId();
    hardware.displayText(deviceStatus.c_str(), 10, 140, 1);

    // Battery status
    String batteryStatus = String("Battery: ") + String(hardware.getBatteryPercentage()) + "%";
    if (hardware.isCharging()) {
        batteryStatus += " (Charging)";
    }
    hardware.displayText(batteryStatus.c_str(), 10, 155, 1);

    // SD Card status
    String sdStatus = "SD Card: ";
    sdStatus += hardware.isSDCardAvailable() ? "Available" : "Not Available";
    hardware.displayText(sdStatus.c_str(), 10, 170, 1);

    // Last update
    String updateStatus = String("Last Update: ") + String((millis() - lastUpdateTime) / 1000) + "s ago";
    hardware.displayText(updateStatus.c_str(), 10, 185, 1);

    // Refresh rate and free RAM
    String refreshStatus = String("Refresh: ") + String(trmnlClient.getRefreshRate()) + "s";
    hardware.displayText(refreshStatus.c_str(), 10, 200, 1);
    String ramLine = String("Free RAM: ") + String(hardware.getFreeHeap()/1024) + " KB";
    hardware.displayText(ramLine.c_str(), 10, 215, 1);

    hardware.displayText("Press B3 to exit | Hold B1: format SD", 10, 235, 1);
    hardware.updateDisplay();

    // Wait loop: B3 exits, long-press B1 formats SD (with beep)
    while (true) {
        hardware.updateButtons();
        if (hardware.getButtonState(2) == BUTTON_PRESSED) { // B3 short press
            hardware.resetButtonState(2);
            break;
        }
        if (hardware.getButtonState(0) == BUTTON_LONG_PRESS) { // B1 long press
            hardware.beep(800, 120);
            hardware.displayText("Formatting SD...", 10, 255, 1);
            hardware.updateDisplay();
            bool ok = hardware.formatSDCard();
            hardware.displayText(ok ? "SD format: OK" : "SD format: FAIL", 10, 270, 1);
            hardware.updateDisplay();
            // consume press
            hardware.resetButtonState(0);
        }
        delay(100);
    }
}

void enterSleepMode() {
    #if DEBUG_ENABLED
    Serial.println("Entering sleep mode...");
    #endif

    // Do NOT change the E-Paper content before sleeping.
    // The current image should remain visible during deep sleep.
    #if DEBUG_ENABLED
    Serial.println("Skipping sleep screen to retain current content on e-paper");
    #endif

    // Disable peripherals to save power
    hardware.disablePeripherals();

    // Calculate sleep duration: exactly the server-provided refresh_rate (seconds)
    uint32_t sleepDuration = trmnlClient.getRefreshRate();

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