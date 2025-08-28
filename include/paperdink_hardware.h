#ifndef PAPERDINK_HARDWARE_H
#define PAPERDINK_HARDWARE_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include "config.h"

// Forward declarations
class GxEPD2_GFX;

// Button states
enum ButtonState {
    BUTTON_RELEASED = 0,
    BUTTON_PRESSED = 1,
    BUTTON_LONG_PRESS = 2,
    BUTTON_VERY_LONG_PRESS = 3
};

// Display types
enum DisplayType {
    DISPLAY_BW = 0,     // Black & White
    DISPLAY_3C = 1      // 3-Color (Black, White, Red)
};

// Power states
enum PowerState {
    POWER_ACTIVE = 0,
    POWER_LIGHT_SLEEP = 1,
    POWER_DEEP_SLEEP = 2
};

class PaperdInkHardware {
private:
    // Display objects
    GxEPD2_GFX* display;
    DisplayType displayType;

    // Button states
    unsigned long buttonPressTime[4];
    ButtonState buttonStates[4];
    bool buttonPressed[4];

    // Power management
    float batteryVoltage;
    bool chargingStatus;
    bool lowBattery;

    // SD Card
    bool sdCardAvailable;

    // Display options
    bool invertDisplayFlag = false;

    // Preferences for persistent storage
    Preferences preferences;

    // Private methods
    void initializePins();
    bool initializeDisplay();
    void initializeSDCard();
    void initializeButtons();
    void updateButtonStates();
    float readBatteryVoltage();
    bool checkChargingStatus();

public:
    PaperdInkHardware();
    ~PaperdInkHardware();

    // Initialization
    bool begin();
    void end();

    // Display methods
    bool initDisplay(DisplayType type = DISPLAY_BW);
    void clearDisplay();
    void updateDisplay();
    void partialUpdateDisplay();
    void displayImage(const uint8_t* imageData, size_t imageSize);
    void displayText(const char* text, int x, int y, int size = 2);
    void displayBitmap(const uint8_t* bitmap, int x, int y, int w, int h);
    void setRotation(int rotation);
    void powerOffDisplay();
    void powerOnDisplay();

    // Display options
    void setInvertDisplay(bool invert);
    bool getInvertDisplay() const;

    // Button methods
    void updateButtons();
    ButtonState getButtonState(int buttonNum);
    bool isButtonPressed(int buttonNum);
    bool isButtonLongPressed(int buttonNum);
    bool isButtonVeryLongPressed(int buttonNum);
    void resetButtonState(int buttonNum);

    // Power management
    void enablePeripherals();
    void disablePeripherals();
    void enterLightSleep(uint32_t sleepTimeMs);
    void enterDeepSleep(uint32_t sleepTimeSeconds);
    float getBatteryVoltage();
    int getBatteryPercentage();
    bool isLowBattery();
    bool isCriticalBattery();
    bool isCharging();

    // SD Card methods
    bool isSDCardAvailable();
    bool writeFile(const char* path, const uint8_t* data, size_t size);
    bool readFile(const char* path, uint8_t* buffer, size_t maxSize, size_t* actualSize);
    bool deleteFile(const char* path);
    bool fileExists(const char* path);
    size_t getFileSize(const char* path);
    bool formatSDCard();  // Danger: deletes all files/directories on SD

    // Buzzer
    void beep(int frequency = 1000, int duration = 100);
    void playTone(int frequency, int duration);

    // Preferences/Settings
    bool saveString(const char* key, const char* value);
    String loadString(const char* key, const char* defaultValue = "");
    bool saveInt(const char* key, int value);
    int loadInt(const char* key, int defaultValue = 0);
    bool saveBool(const char* key, bool value);
    bool loadBool(const char* key, bool defaultValue = false);
    void clearPreferences();

    // Utility methods
    String getMacAddress();
    void restart();
    void factoryReset();
    uint32_t getFreeHeap();
    void printSystemInfo();
};

#endif // PAPERDINK_HARDWARE_H