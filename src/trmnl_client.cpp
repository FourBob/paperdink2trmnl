#include "trmnl_client.h"
#include <Update.h>

TRMNLClient::TRMNLClient(PaperdInkHardware* hw)
    : hardware(hw)
    , configServer(nullptr)
    , dnsServer(nullptr)
    , currentState(STATE_UNINITIALIZED)
    , refreshRate(DEEP_SLEEP_DURATION_SECONDS)
    , configPortalActive(false)
    , configPortalStartTime(0)
    , lastUpdateTime(0)
    , consecutiveErrors(0) {

    macAddress = hardware->getMacAddress();
}

TRMNLClient::~TRMNLClient() {
    end();
}

bool TRMNLClient::begin() {
    #if DEBUG_ENABLED
    Serial.println("Initializing TRMNL client...");
    #endif

    // Load saved device info
    loadDeviceInfo();

    // Configure WiFi client for HTTPS
    wifiClient.setInsecure();  // For now, skip certificate validation

    currentState = STATE_UNINITIALIZED;

    #if DEBUG_ENABLED
    Serial.printf("MAC Address: %s\n", macAddress.c_str());
    Serial.printf("API Key: %s\n", apiKey.length() > 0 ? "Set" : "Not Set");
    Serial.printf("Friendly ID: %s\n", friendlyId.c_str());
    #endif

    return true;
}

void TRMNLClient::end() {
    stopConfigPortal();
    httpClient.end();
    wifiClient.stop();
}

void TRMNLClient::loop() {
    // Handle config portal if active
    if (configPortalActive) {
        handleConfigPortal();

        // Check for timeout
        if (millis() - configPortalStartTime > CONFIG_PORTAL_TIMEOUT_MS) {
            #if DEBUG_ENABLED
            Serial.println("Config portal timeout");
            #endif
            stopConfigPortal();
        }
    }
}

bool TRMNLClient::connectToWiFi() {
    String ssid, password;
    if (!loadCredentials(ssid, password)) {
        #if DEBUG_ENABLED
        Serial.println("No WiFi credentials found");
        #endif
        return false;
    }

    #if DEBUG_ENABLED
    Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    #endif

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        #if DEBUG_ENABLED
        Serial.print(".");
        #endif
    }

    if (WiFi.status() == WL_CONNECTED) {
        #if DEBUG_ENABLED
        Serial.println();
        Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
        #endif
        return true;
    } else {
        #if DEBUG_ENABLED
        Serial.println();
        Serial.println("WiFi connection failed");
        #endif
        return false;
    }
}

bool TRMNLClient::startConfigPortal() {
    if (configPortalActive) {
        return true;
    }

    #if DEBUG_ENABLED
    Serial.println("Starting configuration portal...");
    #endif

    // Create access point
    String apName = "paperdink-setup-" + macAddress.substring(9);  // Last 6 chars of MAC
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apName.c_str(), "paperdink123");

    // Start DNS server for captive portal
    dnsServer = new DNSServer();
    dnsServer->start(53, "*", WiFi.softAPIP());

    // Start web server
    configServer = new WebServer(80);
    configServer->on("/", [this]() { handleRoot(); });
    configServer->on("/save", [this]() { handleWiFiSave(); });
    configServer->on("/reset", [this]() { handleReset(); });
    configServer->onNotFound([this]() { handleRoot(); });  // Captive portal
    configServer->begin();

    configPortalActive = true;
    configPortalStartTime = millis();

    #if DEBUG_ENABLED
    Serial.printf("Config portal started: %s\n", apName.c_str());
    Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    #endif

    return true;
}

void TRMNLClient::stopConfigPortal() {
    if (!configPortalActive) {
        return;
    }

    #if DEBUG_ENABLED
    Serial.println("Stopping configuration portal...");
    #endif

    if (configServer) {
        configServer->stop();
        delete configServer;
        configServer = nullptr;
    }

    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }

    WiFi.softAPdisconnect(true);
    configPortalActive = false;
}

void TRMNLClient::handleConfigPortal() {
    if (dnsServer) {
        dnsServer->processNextRequest();
    }
    if (configServer) {
        configServer->handleClient();
    }
}

void TRMNLClient::handleRoot() {
    String html = generateConfigPage();
    configServer->send(200, "text/html", html);
}

void TRMNLClient::handleWiFiSave() {
    String ssid = configServer->arg("ssid");
    String password = configServer->arg("password");

    if (ssid.length() > 0) {
        saveCredentials(ssid, password);

        String html = "<html><body><h1>WiFi Saved!</h1>";
        html += "<p>SSID: " + ssid + "</p>";
        html += "<p>Device will restart and connect...</p>";
        html += "<script>setTimeout(function(){window.location.href='/';}, 3000);</script>";
        html += "</body></html>";

        configServer->send(200, "text/html", html);

        delay(2000);
        stopConfigPortal();
        hardware->restart();
    } else {
        configServer->send(400, "text/html", "<html><body><h1>Error: SSID required</h1></body></html>");
    }
}

void TRMNLClient::handleReset() {
    clearWiFiCredentials();
    clearDeviceRegistration();

    String html = "<html><body><h1>Factory Reset Complete</h1>";
    html += "<p>Device will restart...</p>";
    html += "</body></html>";

    configServer->send(200, "text/html", html);
    delay(2000);
    hardware->factoryReset();
}

String TRMNLClient::generateConfigPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>paperd.ink TRMNL Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }";
    html += "h1 { color: #333; text-align: center; }";
    html += "input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 5px; }";
    html += "button { width: 100%; padding: 12px; background: #007cba; color: white; border: none; border-radius: 5px; cursor: pointer; margin: 5px 0; }";
    html += "button:hover { background: #005a87; }";
    html += ".info { background: #e7f3ff; padding: 10px; border-radius: 5px; margin: 10px 0; }";
    html += "</style></head><body>";

    html += "<div class='container'>";
    html += "<h1>paperd.ink TRMNL</h1>";
    html += "<div class='info'>";
    html += "<strong>Device Info:</strong><br>";
    html += "MAC: " + macAddress + "<br>";
    html += "Firmware: " + String(FIRMWARE_VERSION) + "<br>";
    html += "Battery: " + String(hardware->getBatteryPercentage()) + "%";
    html += "</div>";

    html += "<form action='/save' method='post'>";
    html += "<h3>WiFi Configuration</h3>";
    html += "<input type='text' name='ssid' placeholder='WiFi Network Name (SSID)' required>";
    html += "<input type='password' name='password' placeholder='WiFi Password'>";
    html += "<button type='submit'>Save WiFi Settings</button>";
    html += "</form>";

    html += "<h3>TRMNL Setup</h3>";
    html += "<p>1. Create account at <a href='https://usetrmnl.com' target='_blank'>usetrmnl.com</a></p>";
    html += "<p>2. Add device with MAC: <strong>" + macAddress + "</strong></p>";
    html += "<p>3. Configure plugins and playlists</p>";

    html += "<button onclick=\"location.href='/reset'\">Factory Reset</button>";
    html += "</div></body></html>";

    return html;
}

// WiFi and credentials management
bool TRMNLClient::hasWiFiCredentials() {
    String ssid, password;
    return loadCredentials(ssid, password);
}

void TRMNLClient::saveCredentials(const String& ssid, const String& password) {
    hardware->saveString("wifi_ssid", ssid.c_str());
    hardware->saveString("wifi_password", password.c_str());

    #if DEBUG_ENABLED
    Serial.printf("WiFi credentials saved: %s\n", ssid.c_str());
    #endif
}

bool TRMNLClient::loadCredentials(String& ssid, String& password) {
    #ifdef WIFI_SSID
    // Use hardcoded credentials from secrets.h if available
    ssid = WIFI_SSID;
    #ifdef WIFI_PASSWORD
    password = WIFI_PASSWORD;
    #else
    password = "";
    #endif
    return true;
    #else
    // Load from preferences
    ssid = hardware->loadString("wifi_ssid", "");
    password = hardware->loadString("wifi_password", "");
    return ssid.length() > 0;
    #endif
}

void TRMNLClient::clearWiFiCredentials() {
    hardware->saveString("wifi_ssid", "");
    hardware->saveString("wifi_password", "");
}

// Device registration
bool TRMNLClient::isDeviceRegistered() {
    return apiKey.length() > 0 && friendlyId.length() > 0;
}

void TRMNLClient::saveDeviceInfo() {
    hardware->saveString("api_key", apiKey.c_str());
    hardware->saveString("friendly_id", friendlyId.c_str());
    hardware->saveInt("refresh_rate", refreshRate);
}

bool TRMNLClient::loadDeviceInfo() {
    apiKey = hardware->loadString("api_key", "");
    friendlyId = hardware->loadString("friendly_id", "");
    refreshRate = hardware->loadInt("refresh_rate", DEEP_SLEEP_DURATION_SECONDS);

    return isDeviceRegistered();
}

void TRMNLClient::clearDeviceRegistration() {
    apiKey = "";
    friendlyId = "";
    hardware->saveString("api_key", "");
    hardware->saveString("friendly_id", "");
}

// WiFi management functions
bool TRMNLClient::isWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String TRMNLClient::getWiFiSSID() {
    return WiFi.SSID();
}

int TRMNLClient::getWiFiRSSI() {
    return WiFi.RSSI();
}

// Device registration
bool TRMNLClient::registerDevice() {
    if (!connectToWiFi()) {
        lastError = "WiFi connection failed";
        return false;
    }

    SetupResponse response;
    if (callSetupAPI(response)) {
        apiKey = response.apiKey;
        friendlyId = response.friendlyId;
        saveDeviceInfo();
        setState(STATE_OPERATIONAL);
        return true;
    }

    lastError = "Device registration failed";
    return false;
}

// Content management
bool TRMNLClient::updateContent() {
    if (!isWiFiConnected()) {
        setState(STATE_OFFLINE);
        return false;
    }

    DisplayResponse response;
    if (callDisplayAPI(response)) {
        if (response.imageUrl.length() > 0) {
            // Download and display image
            uint8_t* imageBuffer = (uint8_t*)malloc(MAX_IMAGE_SIZE);
            if (imageBuffer) {
                size_t imageSize;
                if (downloadImage(response.imageUrl, imageBuffer, MAX_IMAGE_SIZE, &imageSize)) {
                    hardware->displayImage(imageBuffer, imageSize);

                    // Cache the image
                    if (CACHE_ENABLED && response.filename.length() > 0) {
                        cacheImage(response.filename, imageBuffer, imageSize);
                    }

                    lastImageFilename = response.filename;
                    lastUpdateTime = millis();
                    consecutiveErrors = 0;
                    free(imageBuffer);
                    return true;
                }
                free(imageBuffer);
            }
        }
    }

    consecutiveErrors++;
    return false;
}

bool TRMNLClient::displayContent() {
    return updateContent();
}

bool TRMNLClient::hasNewContent() {
    // Simple implementation - could be enhanced with server polling
    return true;
}

void TRMNLClient::forceRefresh() {
    updateContent();
}

// Offline mode
bool TRMNLClient::enterOfflineMode() {
    setState(STATE_OFFLINE);
    return displayCachedContent();
}

bool TRMNLClient::displayCachedContent() {
    if (!hardware->isSDCardAvailable() || lastImageFilename.length() == 0) {
        return false;
    }

    uint8_t* imageBuffer = (uint8_t*)malloc(MAX_IMAGE_SIZE);
    if (imageBuffer) {
        size_t imageSize;
        if (loadCachedImage(lastImageFilename, imageBuffer, MAX_IMAGE_SIZE, &imageSize)) {
            hardware->displayImage(imageBuffer, imageSize);
            free(imageBuffer);
            return true;
        }
        free(imageBuffer);
    }

    return false;
}

bool TRMNLClient::hasCachedContent() {
    return hardware->isSDCardAvailable() &&
           lastImageFilename.length() > 0 &&
           isCacheValid(lastImageFilename);
}

// Settings
void TRMNLClient::setRefreshRate(int seconds) {
    refreshRate = seconds;
    hardware->saveInt("refresh_rate", refreshRate);
}

// Error handling
void TRMNLClient::clearErrors() {
    consecutiveErrors = 0;
    lastError = "";
}

// Utility functions
void TRMNLClient::printStatus() {
    #if DEBUG_ENABLED
    Serial.println("=== TRMNL Client Status ===");
    Serial.printf("State: %d\n", currentState);
    Serial.printf("WiFi: %s\n", isWiFiConnected() ? "Connected" : "Disconnected");
    Serial.printf("API Key: %s\n", apiKey.length() > 0 ? "Set" : "Not Set");
    Serial.printf("Friendly ID: %s\n", friendlyId.c_str());
    Serial.printf("Refresh Rate: %d seconds\n", refreshRate);
    Serial.printf("Consecutive Errors: %d\n", consecutiveErrors);
    Serial.printf("Last Error: %s\n", lastError.c_str());
    Serial.println("==========================");
    #endif
}

String TRMNLClient::getStatusString() {
    String status = "State: ";
    switch (currentState) {
        case STATE_UNINITIALIZED: status += "Uninitialized"; break;
        case STATE_WIFI_SETUP: status += "WiFi Setup"; break;
        case STATE_DEVICE_SETUP: status += "Device Setup"; break;
        case STATE_OPERATIONAL: status += "Operational"; break;
        case STATE_ERROR: status += "Error"; break;
        case STATE_OFFLINE: status += "Offline"; break;
        default: status += "Unknown"; break;
    }
    return status;
}

bool TRMNLClient::performSelfTest() {
    // Basic self-test implementation
    if (!hardware) return false;
    if (!isWiFiConnected()) return false;
    if (!isDeviceRegistered()) return false;
    return true;
}

// API implementation
bool TRMNLClient::callSetupAPI(SetupResponse& response) {
    if (!isWiFiConnected()) return false;

    httpClient.begin(wifiClient, String(TRMNL_API_BASE_URL) + TRMNL_API_SETUP_ENDPOINT);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("User-Agent", "paperdink-trmnl/1.0");

    // Create setup request
    JsonDocument doc;
    doc["mac"] = macAddress;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["device_type"] = "paperdink";

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = httpClient.POST(requestBody);

    if (httpResponseCode == 200) {
        String responseBody = httpClient.getString();
        JsonDocument responseDoc;

        if (deserializeJson(responseDoc, responseBody) == DeserializationError::Ok) {
            response.status = responseDoc["status"] | 0;
            response.apiKey = responseDoc["api_key"] | "";
            response.friendlyId = responseDoc["friendly_id"] | "";
            response.imageUrl = responseDoc["image_url"] | "";
            response.filename = responseDoc["filename"] | "";

            httpClient.end();
            return response.status == 200;
        }
    }

    httpClient.end();
    return false;
}

bool TRMNLClient::callDisplayAPI(DisplayResponse& response) {
    if (!isWiFiConnected() || apiKey.length() == 0) return false;

    httpClient.begin(wifiClient, String(TRMNL_API_BASE_URL) + TRMNL_API_DISPLAY_ENDPOINT);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("Authorization", "Bearer " + apiKey);
    httpClient.addHeader("User-Agent", "paperdink-trmnl/1.0");

    int httpResponseCode = httpClient.GET();

    if (httpResponseCode == 200) {
        String responseBody = httpClient.getString();
        JsonDocument responseDoc;

        if (deserializeJson(responseDoc, responseBody) == DeserializationError::Ok) {
            response.status = responseDoc["status"] | 0;
            response.imageUrl = responseDoc["image_url"] | "";
            response.filename = responseDoc["filename"] | "";
            response.updateFirmware = responseDoc["update_firmware"] | false;
            response.firmwareUrl = responseDoc["firmware_url"] | "";
            response.refreshRate = responseDoc["refresh_rate"] | refreshRate;
            response.resetFirmware = responseDoc["reset_firmware"] | false;
            response.error = responseDoc["error"] | "";

            // Update refresh rate if provided
            if (response.refreshRate != refreshRate) {
                setRefreshRate(response.refreshRate);
            }

            httpClient.end();
            return response.status == 200;
        }
    }

    httpClient.end();
    return false;
}

bool TRMNLClient::downloadImage(const String& imageUrl, uint8_t* buffer, size_t maxSize, size_t* actualSize) {
    if (!isWiFiConnected() || imageUrl.length() == 0) return false;

    httpClient.begin(wifiClient, imageUrl);
    httpClient.addHeader("User-Agent", "paperdink-trmnl/1.0");

    int httpResponseCode = httpClient.GET();

    if (httpResponseCode == 200) {
        WiFiClient* stream = httpClient.getStreamPtr();
        size_t bytesRead = 0;

        while (httpClient.connected() && bytesRead < maxSize) {
            size_t available = stream->available();
            if (available) {
                size_t toRead = min(available, maxSize - bytesRead);
                size_t read = stream->readBytes(buffer + bytesRead, toRead);
                bytesRead += read;
            } else {
                delay(1);
            }
        }

        if (actualSize) {
            *actualSize = bytesRead;
        }

        httpClient.end();
        return bytesRead > 0;
    }

    httpClient.end();
    return false;
}

bool TRMNLClient::sendLogs(const String& logData) {
    if (!isWiFiConnected() || apiKey.length() == 0) return false;

    httpClient.begin(wifiClient, String(TRMNL_API_BASE_URL) + TRMNL_API_LOGS_ENDPOINT);
    httpClient.addHeader("Content-Type", "application/json");
    httpClient.addHeader("Authorization", "Bearer " + apiKey);
    httpClient.addHeader("User-Agent", "paperdink-trmnl/1.0");

    JsonDocument doc;
    doc["mac"] = macAddress;
    doc["logs"] = logData;
    doc["timestamp"] = millis();

    String requestBody;
    serializeJson(doc, requestBody);

    int httpResponseCode = httpClient.POST(requestBody);
    httpClient.end();

    return httpResponseCode == 200;
}

// Cache implementation
bool TRMNLClient::cacheImage(const String& filename, const uint8_t* imageData, size_t imageSize) {
    if (!hardware->isSDCardAvailable()) return false;

    String cachePath = "/cache/" + filename;
    return hardware->writeFile(cachePath.c_str(), imageData, imageSize);
}

bool TRMNLClient::loadCachedImage(const String& filename, uint8_t* buffer, size_t maxSize, size_t* actualSize) {
    if (!hardware->isSDCardAvailable()) return false;

    String cachePath = "/cache/" + filename;
    return hardware->readFile(cachePath.c_str(), buffer, maxSize, actualSize);
}

bool TRMNLClient::isCacheValid(const String& filename) {
    if (!hardware->isSDCardAvailable()) return false;

    String cachePath = "/cache/" + filename;
    return hardware->fileExists(cachePath.c_str());
}

void TRMNLClient::cleanupCache() {
    // Simple cache cleanup - could be enhanced
    if (!hardware->isSDCardAvailable()) return;

    // TODO: Implement cache cleanup logic
    #if DEBUG_ENABLED
    Serial.println("Cache cleanup not implemented yet");
    #endif
}

// Firmware update functions
bool TRMNLClient::checkForFirmwareUpdate() {
    DisplayResponse response;
    if (callDisplayAPI(response)) {
        return response.updateFirmware && response.firmwareUrl.length() > 0;
    }
    return false;
}

bool TRMNLClient::performFirmwareUpdate(const String& firmwareUrl) {
    // OTA update implementation would go here
    #if DEBUG_ENABLED
    Serial.printf("Firmware update requested: %s\n", firmwareUrl.c_str());
    Serial.println("OTA update not implemented yet");
    #endif
    return false;
}

bool TRMNLClient::downloadFirmware(const String& firmwareUrl) {
    return performFirmwareUpdate(firmwareUrl);
}