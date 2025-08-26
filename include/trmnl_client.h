#ifndef TRMNL_CLIENT_H
#define TRMNL_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "paperdink_hardware.h"

// TRMNL API Response Status Codes
enum TRMNLStatus {
    TRMNL_SUCCESS = 0,
    TRMNL_NO_USER = 202,
    TRMNL_NOT_FOUND = 404,
    TRMNL_ERROR = 500
};

// Device states
enum DeviceState {
    STATE_UNINITIALIZED = 0,
    STATE_WIFI_SETUP = 1,
    STATE_DEVICE_SETUP = 2,
    STATE_OPERATIONAL = 3,
    STATE_ERROR = 4,
    STATE_OFFLINE = 5
};

// API Response structures
struct SetupResponse {
    int status;
    String apiKey;
    String friendlyId;
    String imageUrl;
    String filename;
};

struct DisplayResponse {
    int status;
    String imageUrl;
    String filename;
    bool updateFirmware;
    String firmwareUrl;
    int refreshRate;
    bool resetFirmware;
    String error;
};

class TRMNLClient {
private:
    // Hardware reference
    PaperdInkHardware* hardware;

    // Network components
    WiFiClientSecure wifiClient;
    HTTPClient httpClient;
    WebServer* configServer;
    DNSServer* dnsServer;

    // Device state
    DeviceState currentState;
    String macAddress;
    String apiKey;
    String friendlyId;
    int refreshRate;

    // Configuration portal
    bool configPortalActive;
    unsigned long configPortalStartTime;

    // Cache management
    String lastImageFilename;
    unsigned long lastUpdateTime;

    // Error handling
    int consecutiveErrors;
    String lastError;

    // Private methods
    bool connectToWiFi();
    bool setupConfigPortal();
    void handleConfigPortal();
    void handleRoot();
    void handleWiFiSave();
    void handleReset();
    String generateConfigPage();

    // API methods
    bool callSetupAPI(SetupResponse& response);
    bool callDisplayAPI(DisplayResponse& response);
    bool sendLogs(const String& logData);
    bool downloadImage(const String& imageUrl, uint8_t* buffer, size_t maxSize, size_t* actualSize);
    bool downloadFirmware(const String& firmwareUrl);

    // Utility methods
    String createRequestHeaders(bool includeAuth = false);
    bool parseJsonResponse(const String& json, JsonDocument& doc);
    void saveCredentials(const String& ssid, const String& password);
    bool loadCredentials(String& ssid, String& password);
    void saveDeviceInfo();
    bool loadDeviceInfo();

    // Cache methods
    bool cacheImage(const String& filename, const uint8_t* imageData, size_t imageSize);
    bool loadCachedImage(const String& filename, uint8_t* buffer, size_t maxSize, size_t* actualSize);
    bool isCacheValid(const String& filename);
    void cleanupCache();

public:
    TRMNLClient(PaperdInkHardware* hw);
    ~TRMNLClient();

    // Initialization and lifecycle
    bool begin();
    void end();
    void loop();

    // State management
    DeviceState getState() const { return currentState; }
    void setState(DeviceState state) { currentState = state; }

    // WiFi management
    bool isWiFiConnected();
    bool startConfigPortal();
    void stopConfigPortal();
    bool hasWiFiCredentials();
    void clearWiFiCredentials();
    String getWiFiSSID();
    int getWiFiRSSI();

    // Device setup and registration
    bool registerDevice();
    bool isDeviceRegistered();
    void clearDeviceRegistration();
    String getApiKey() const { return apiKey; }
    String getFriendlyId() const { return friendlyId; }

    // Content management
    bool updateContent();
    bool displayContent();
    bool hasNewContent();
    void forceRefresh();

    // Offline mode
    bool enterOfflineMode();
    bool displayCachedContent();
    bool hasCachedContent();

    // Settings
    void setRefreshRate(int seconds);
    int getRefreshRate() const { return refreshRate; }

    // Error handling
    String getLastError() const { return lastError; }
    int getConsecutiveErrors() const { return consecutiveErrors; }
    void clearErrors();

    // Firmware updates
    bool checkForFirmwareUpdate();
    bool performFirmwareUpdate(const String& firmwareUrl);

    // Utility
    void printStatus();
    String getStatusString();
    bool performSelfTest();
};

#endif // TRMNL_CLIENT_H