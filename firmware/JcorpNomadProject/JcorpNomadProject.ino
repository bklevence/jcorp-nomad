// Core ESP32 + WiFi Libraries
#include "Arduino.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include "SD_MMC.h"
#include "DNSServer.h"

// Waveshare Display and UI Libraries
#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "ui.h"
#include "RGB_lamp.h"

// SPIFFS support and ESP WiFi control
#include <SPIFFS.h>
#include "esp_wifi.h"

// ==================== CONFIGURATION ====================

// WiFi network name and password (this is the hotspot the ESP32 creates)
#define WIFI_SSID "Jcorp_Nomad"
#define WIFI_PASSWORD "password"

// Max number of devices that can connect at once
#define MAX_CLIENTS 8

// SD card pinout for Waveshare ESP32-S3
#define SD_CLK_PIN 14
#define SD_CMD_PIN 15
#define SD_D0_PIN 16
#define SD_D1_PIN 18
#define SD_D2_PIN 17
#define SD_D3_PIN 21

// Captive portal DNS setup
const byte DNS_PORT = 53;
DNSServer dnsServer;
AsyncWebServer server(80); // Web server on port 80

int connectedClients = 0;

// Update the UI with the number of connected users
void updateUI(int userCount) {
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%d", userCount);
    lv_label_set_text(ui_uiuserlabel, buffer);
}

// Get a count of currently connected WiFi clients
void updateClientCount() {
    wifi_sta_list_t wifi_sta_list;
    if (esp_wifi_ap_get_sta_list(&wifi_sta_list) == ESP_OK) {
        connectedClients = wifi_sta_list.num;
        updateUI(connectedClients);
    }
}

// ==================== MEDIA STREAM HANDLER ====================

// Handles video/audio streaming via range requests
void handleRangeRequest(AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "File parameter missing");
        return;
    }

    String filePath = request->getParam("file")->value();
    if (!SD_MMC.exists(filePath)) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    File file = SD_MMC.open(filePath, "r");
    size_t fileSize = file.size();

    Serial.println("=== Range Request Debug ===");
    Serial.println("File requested: " + filePath);
    Serial.println("File size: " + String(fileSize));

    if (request->method() == HTTP_HEAD) {
        Serial.println("HEAD request received");
        AsyncWebServerResponse *headResponse = request->beginResponse(200, "application/octet-stream", "");
        headResponse->addHeader("Content-Length", String(fileSize));
        request->send(headResponse);
        file.close();
        return;
    }

    // Handle "Range" header to support seeking and chunked media playback
    String rangeHeader = request->header("Range");
    Serial.println("Raw Range header: " + rangeHeader);

    size_t startByte = 0, endByte = fileSize - 1;
    if (rangeHeader.startsWith("bytes=")) {
        int dashIndex = rangeHeader.indexOf('-');
        startByte = rangeHeader.substring(6, dashIndex).toInt();
        if (dashIndex + 1 < rangeHeader.length()) {
            endByte = rangeHeader.substring(dashIndex + 1).toInt();
        }
    }
    if (endByte >= fileSize) endByte = fileSize - 1;
    size_t contentLength = endByte - startByte + 1;

    Serial.println("Computed startByte: " + String(startByte));
    Serial.println("Computed endByte: " + String(endByte));
    Serial.println("Content length: " + String(contentLength));
    Serial.println("============================");

    AsyncWebServerResponse *response = request->beginResponse("video/mp4", contentLength,
        [file, startByte, endByte, contentLength](uint8_t *buffer, size_t maxLen, size_t index) mutable -> size_t {
            file.seek(startByte + index);
            size_t bytesToRead = min(maxLen, endByte - (startByte + index) + 1);
            size_t bytesRead = file.read(buffer, bytesToRead);
            if (index + bytesRead >= contentLength) {
                file.close();
            }
            return bytesRead;
        }
    );

    response->addHeader("Content-Type", "video/mp4");
    response->addHeader("Accept-Ranges", "bytes");
    response->addHeader("Content-Range", "bytes " + String(startByte) + "-" + String(endByte) + "/" + String(fileSize));
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Pragma", "no-cache");
    response->setCode(206); // Partial Content
    request->send(response);
}

// ==================== SETUP ====================

void setup() {
    LCD_Init();
    Lvgl_Init();
    Set_Backlight(90); // Set display brightness
    ui_init();         // Load the GUI

    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32-S3 Captive Portal & SDMMC Server ===");

    // Start WiFi Access Point
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("WiFi AP started...");

    // Initialize SD card
    Serial.println("Initializing SD Card...");
    if (!SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN, SD_D1_PIN, SD_D2_PIN, SD_D3_PIN)) {
        Serial.println("ERROR: SDMMC Pin configuration failed!");
        return;
    }
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("ERROR: SDMMC Card initialization failed.");
        return;
    }
    Serial.println("SD Card initialized successfully!");

    // Start Captive DNS redirection
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Captive Portal: Redirect all unknown requests
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->hasHeader("User-Agent")) {
            String userAgent = request->header("User-Agent");
            Serial.println("User-Agent: " + userAgent);

            if (userAgent.indexOf("iPhone") >= 0 || userAgent.indexOf("iPad") >= 0 || userAgent.indexOf("Macintosh") >= 0) {
                Serial.println("Apple device detected. Serving appleindex.html");
                request->send(SD_MMC, "/appleindex.html", "text/html");
                return;
            }
        }
        Serial.println("Non-Apple device. Serving index.html");
        request->send(SD_MMC, "/index.html", "text/html");
    });

    // Captive triggers for Apple & Android devices
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Apple captive portal request detected, serving appleindex.html");
        request->send(SD_MMC, "/appleindex.html", "text/html");
    });

    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Android/NORMAL captive portal request detected, serving index.html");
        request->send(SD_MMC, "/index.html", "text/html");
    });

    // Static HTML routes
    server.serveStatic("/movies.html", SD_MMC, "/movies.html");
    server.serveStatic("/music.html", SD_MMC, "/music.html");
    server.serveStatic("/books.html", SD_MMC, "/books.html");
    server.serveStatic("/shows.html", SD_MMC, "/shows.html");

    // Serve root directory and default to index.html
    server.serveStatic("/", SD_MMC, "/").setDefaultFile("index.html");

    // Endpoint for video/audio streaming
    server.on("/media", HTTP_GET, handleRangeRequest);

    // Start the web server
    server.begin();
    Serial.println("Web Server started!");
}

// ==================== MAIN LOOP ====================

void loop() {
    dnsServer.processNextRequest(); // Keeps captive DNS alive
    Timer_Loop();                   // Handles UI timing events
    RGB_Lamp_Loop(2);               // Optional LED effect loop
    delay(5);                       // Slight delay

    updateClientCount();           // Refresh user count
}
