#ifndef CONFIG_H
#define CONFIG_H

// Include secrets (WiFi credentials, API keys, etc.)
#include "secrets.h"

// Firmware Version
#define FIRMWARE_VERSION "1.0.0-paperdink"

// TRMNL API Configuration
#ifndef TRMNL_API_BASE_URL
#define TRMNL_API_BASE_URL "https://usetrmnl.com"
#endif

#define TRMNL_API_SETUP_ENDPOINT "/api/setup"
#define TRMNL_API_DISPLAY_ENDPOINT "/api/display"
#define TRMNL_API_LOGS_ENDPOINT "/api/logs"

// paperd.ink Hardware Pin Definitions
// I2C Pins
#define SDA_PIN 16
#define SCL_PIN 17

// SPI Pins
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19

// SD Card Pins
#define SD_CS_PIN 21
#define SD_ENABLE_PIN 5

// E-paper Display Pins
#define EPD_CS_PIN 22
#define EPD_DC_PIN 15
#define EPD_BUSY_PIN 34
#define EPD_RESET_PIN 13
#define EPD_ENABLE_PIN 12

// PCF8574 Pins
#define PCF_INT_PIN 35
#define PCF_I2C_ADDR 0x20  // Rev 3: 0x20, Rev 4: 0x38

// Button Pins (via PCF8574 or direct GPIO)
#define BUTTON_1_PIN 14  // Top button
#define BUTTON_2_PIN 27  // Second button
#define BUTTON_3_PIN 4   // Third button
#define BUTTON_4_PIN 2   // Bottom button

// Battery and Power Management
#define BATTERY_ENABLE_PIN 25
#define BATTERY_VOLTAGE_PIN 39
#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_3
#define CHARGING_INDICATOR_PIN 36

// Buzzer
#define BUZZER_PIN 26

// Display Configuration
#define DISPLAY_WIDTH 400
#define DISPLAY_HEIGHT 300
#define DISPLAY_ROTATION 0

// Power Management
#define DEEP_SLEEP_DURATION_SECONDS 1800  // 30 minutes default
#define LOW_BATTERY_THRESHOLD 3.2  // Volts
#define CRITICAL_BATTERY_THRESHOLD 3.0  // Volts

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_MAX_RETRIES 5
#define CONFIG_PORTAL_TIMEOUT_MS 300000  // 5 minutes

// TRMNL Client Configuration
#define HTTP_TIMEOUT_MS 30000
#define MAX_IMAGE_SIZE 50000  // 50KB max image size
#define CACHE_ENABLED true
#define MAX_CACHED_IMAGES 10

// Button Configuration
#define BUTTON_DEBOUNCE_MS 50
#define BUTTON_LONG_PRESS_MS 2000
#define BUTTON_VERY_LONG_PRESS_MS 5000

// Debug Configuration
#ifdef CORE_DEBUG_LEVEL
#define DEBUG_ENABLED true
#define DEBUG_SERIAL_SPEED 115200
#else
#define DEBUG_ENABLED false
#endif

#endif // CONFIG_H