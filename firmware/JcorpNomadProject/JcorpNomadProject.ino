//Jcorp Nomad Project
#include "Arduino.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"
#include "SD_MMC.h"
#include "DNSServer.h"
#include <ArduinoJson.h>
#include <map>
#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "ui.h"
#include "RGB_lamp.h"
#include <SPIFFS.h>
#include "esp_wifi.h"

// ==================== CONFIGURATION ====================

// WiFi network name and password (this is the hotspot the ESP32 creates)
#define WIFI_SSID "Jcorp_Nomad"
#define WIFI_PASSWORD "password"

// Max number of devices that can connect at once
#define MAX_CLIENTS 4
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
std::map<AsyncWebServerRequest*, File> activeUploads;
int connectedClients = 0;
// LED Mode and Color Helper Wrappers
uint8_t currentLEDMode = 0;  // 0=off, 1=rainbow, 2=solid color
uint8_t solidR = 0, solidG = 0, solidB = 0;
void RGB_SetColor(uint8_t r, uint8_t g, uint8_t b) {
    solidR = r;
    solidG = g;
    solidB = b;
    currentLEDMode = 2; 
    Set_Color(g, r, b);
}
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
void generateMediaJSON() {
    Serial.println("[MediaGen] Called generateMediaJSON()");

    if (SD_MMC.exists("/media.json")) {
        SD_MMC.remove("/media.json");
        Serial.println("[MediaGen] Old media.json removed.");
    }

    StaticJsonDocument<32768> doc;

    // Movies
    JsonArray movies = doc.createNestedArray("movies");
    File moviesDir = SD_MMC.open("/Movies");
    while (File file = moviesDir.openNextFile()) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".mp4") || name.endsWith(".mkv")) {
                Serial.println("[MediaGen] Found movie: " + name);
                String base = name.substring(name.lastIndexOf('/') + 1);
                base = base.substring(0, base.lastIndexOf('.'));

                String cover = "movies/" + base + ".jpg";
                if (!SD_MMC.exists("/Movies/" + base + ".jpg")) {
                    cover = "placeholder.jpg";
                }

                JsonObject item = movies.createNestedObject();
                item["name"] = base;
                item["cover"] = cover;
                item["file"] = "Movies/" + base + name.substring(name.lastIndexOf('.'));  // Ensure full filename
            }
        }
    }

    // Shows
    JsonArray shows = doc.createNestedArray("shows");
    File showsRoot = SD_MMC.open("/Shows");

    while (File folder = showsRoot.openNextFile()) {
        if (folder.isDirectory()) {
            String folderName = String(folder.name()); // just "GravityFalls"
            String fullPath = "/Shows/" + folderName;

            Serial.println("[MediaGen] Found show folder: " + folderName);

            JsonObject show = shows.createNestedObject();
            show["name"] = folderName;

            String coverPath = "shows/" + folderName + ".jpg";
            if (!SD_MMC.exists("/Shows/" + folderName + ".jpg")) {
                coverPath = "placeholder.jpg";
            }
            show["cover"] = coverPath;

            JsonArray episodes = show.createNestedArray("episodes");

            File epFolder = SD_MMC.open(fullPath);
            while (File ep = epFolder.openNextFile()) {
                if (!ep.isDirectory()) {
                    String epName = String(ep.name());
                    if (epName.endsWith(".mp4") || epName.endsWith(".mkv")) {
                        String epBase = epName.substring(epName.lastIndexOf('/') + 1);

                        JsonObject epItem = episodes.createNestedObject();
                        epItem["name"] = epBase;
                        epItem["file"] = "Shows/" + folderName + "/" + epBase;

                        Serial.println("  [MediaGen] Episode: " + epBase);
                    }
                }
            }
        }
    }

    // Books
    JsonArray books = doc.createNestedArray("books");
    File booksDir = SD_MMC.open("/Books");
    while (File file = booksDir.openNextFile()) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".pdf") || name.endsWith(".epub")) {
                Serial.println("[MediaGen] Found book: " + name);
                String base = name.substring(name.lastIndexOf('/') + 1);
                base = base.substring(0, base.lastIndexOf('.'));

                String cover = "books/" + base + ".jpg";
                if (!SD_MMC.exists("/Books/" + base + ".jpg")) {
                    cover = "placeholder.jpg";
                }

                JsonObject item = books.createNestedObject();
                item["name"] = base;
                item["cover"] = cover;
                item["file"] = "Books/" + base + name.substring(name.lastIndexOf('.'));
            }
        }
    }

    // Music
    JsonArray music = doc.createNestedArray("music");
    File musicDir = SD_MMC.open("/Music");
    while (File file = musicDir.openNextFile()) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".mp3")) {
                Serial.println("[MediaGen] Found music: " + name);
                String base = name.substring(name.lastIndexOf('/') + 1);
                base = base.substring(0, base.lastIndexOf('.'));

                String cover = "music/" + base + ".jpg";
                if (!SD_MMC.exists("/Music/" + base + ".jpg")) {
                    cover = "placeholder.jpg";
                }

                JsonObject item = music.createNestedObject();
                item["name"] = base;
                item["cover"] = cover;
                item["file"] = "Music/" + base + ".mp3";
            }
        }
    }

    // Save to media.json
    File jsonFile = SD_MMC.open("/media.json", FILE_WRITE);
    if (!jsonFile) {
        Serial.println("[MediaGen] Error opening media.json for writing.");
        return;
    }

    serializeJsonPretty(doc, jsonFile);
    jsonFile.close();
    Serial.println("[MediaGen] media.json created successfully.");
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
void handleListFiles(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir")) {
        request->send(400, "application/json", "{\"error\":\"Missing 'dir' parameter\"}");
        return;
    }
    String dir = request->getParam("dir")->value();

    if (!SD_MMC.exists(dir)) {
        request->send(404, "application/json", "{\"error\":\"Directory not found\"}");
        return;
    }

    File directory = SD_MMC.open(dir);
    if (!directory || !directory.isDirectory()) {
        request->send(404, "application/json", "{\"error\":\"Not a directory\"}");
        return;
    }

    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();

    File file = directory.openNextFile();
    while (file) {
        JsonObject f = arr.createNestedObject();

        String filename = String(file.name());
        // Strip dir prefix
        if (filename.startsWith(dir)) {
            filename = filename.substring(dir.length());
            if (filename.startsWith("/")) filename = filename.substring(1);
        }

        if (file.isDirectory()) {
            // Append slash to indicate directory
            f["name"] = filename + "/";
            f["size"] = 0;
            f["isDir"] = true;
        } else {
            f["name"] = filename;
            f["size"] = file.size();
            f["isDir"] = false;
        }
        file = directory.openNextFile();
    }
    directory.close();

    String response;
    serializeJson(arr, response);
    request->send(200, "application/json", response);
}

void handleFileUpload(AsyncWebServerRequest *request) {
    if (!request->hasParam("dir", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing 'dir' form field\"}");
        return;
    }
    String dir = request->getParam("dir", true)->value();

    if (!SD_MMC.exists(dir) || !SD_MMC.open(dir).isDirectory()) {
        request->send(404, "application/json", "{\"error\":\"Directory not found\"}");
        return;
    }

    request->send(200, "application/json", "{\"status\":\"Ready to upload\"}");
}
void handleRename(AsyncWebServerRequest *request) {
    if (!request->hasParam("oldname", true) || !request->hasParam("newname", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing parameters\"}");
        return;
    }

    String oldName = request->getParam("oldname", true)->value();
    String newName = request->getParam("newname", true)->value();

    if (!SD_MMC.exists(oldName)) {
        request->send(404, "application/json", "{\"error\":\"Original file not found\"}");
        return;
    }

    if (SD_MMC.exists(newName)) {
        request->send(409, "application/json", "{\"error\":\"Target file already exists\"}");
        return;
    }

    if (SD_MMC.rename(oldName, newName)) {
        request->send(200, "application/json", "{\"status\":\"Rename successful\"}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Rename failed\"}");
    }
}
void handleDelete(AsyncWebServerRequest *request) {
    if (!request->hasParam("filename", true)) {
        request->send(400, "application/json", "{\"error\":\"Missing 'filename' parameter\"}");
        return;
    }

    String filename = request->getParam("filename", true)->value();

    if (!SD_MMC.exists(filename)) {
        request->send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }

    if (SD_MMC.remove(filename)) {
        request->send(200, "application/json", "{\"status\":\"Delete successful\"}");
    } else {
        request->send(500, "application/json", "{\"error\":\"Delete failed\"}");
    }
}

std::map<AsyncWebServerRequest*, String> uploadPaths;
File uploadFile;

void onUploadHandler(AsyncWebServerRequest *request, const String& filename, size_t index,
                     uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        // Read subdir from URL query (e.g. /upload-show?subdir=MyShow)
        String subdir = "/";
        if (request->hasParam("subdir")) {
            subdir = request->getParam("subdir")->value();
        }

        // Sanitize subdir path
        if (!subdir.startsWith("/")) subdir = "/" + subdir;
        if (subdir.endsWith("/"))  subdir.remove(subdir.length() - 1);

        // Build the full SD path under /Shows
        String fullPath = "/Shows" + subdir + "/" + filename;

        // Ensure parent directory exists
        int slashPos = fullPath.lastIndexOf('/');
        if (slashPos != -1) {
            String parent = fullPath.substring(0, slashPos);
            SD_MMC.mkdir(parent);
        }

        Serial.println("[Upload] Starting upload to: " + fullPath);
        uploadFile = SD_MMC.open(fullPath, FILE_WRITE);
        if (!uploadFile) {
            Serial.println("[Upload] Failed to open: " + fullPath);
            return;
        }
        uploadPaths[request] = fullPath;
    }

    // Write chunk
    if (uploadFile) {
        uploadFile.write(data, len);
    }

    // Finalize
    if (final) {
        Serial.println("[Upload] Complete: " + uploadPaths[request]);
        uploadFile.close();
        uploadPaths.erase(request);
    }
}


void createSimpleUploadHandler(const String& mediaFolder, const char* endpoint) {
    server.on(endpoint, HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"status\":\"Upload finished\"}");
        },
        [mediaFolder](AsyncWebServerRequest *request, const String& filename, size_t index,
                      uint8_t *data, size_t len, bool final) {

            if (index == 0) {
                String fullPath = "/" + mediaFolder + "/" + filename;
                Serial.println("[Upload] Starting upload to: " + fullPath);
                File f = SD_MMC.open(fullPath, FILE_WRITE);
                if (!f) {
                    Serial.println("[Upload] Failed to open file for writing");
                    return;
                }
                activeUploads[request] = f;
            }

            if (activeUploads.count(request)) {
                activeUploads[request].write(data, len);
                Serial.printf("[Upload] Written %u bytes to %s\n", len, filename.c_str());
            }

            if (final && activeUploads.count(request)) {
                Serial.println("[Upload] Upload complete for: " + filename);
                activeUploads[request].close();
                activeUploads.erase(request);
            }
        }
    );
}void handleSDInfo(AsyncWebServerRequest *request) {
    uint64_t totalBytes = SD_MMC.totalBytes();
    uint64_t usedBytes  = SD_MMC.usedBytes();

    DynamicJsonDocument doc(256);
    doc["total"] = totalBytes;
    doc["used"] = usedBytes;

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
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
    createSimpleUploadHandler("Movies", "/upload-movie");
    createSimpleUploadHandler("Music", "/upload-music");
    createSimpleUploadHandler("Books", "/upload-book");

    delay(2000); 

    generateMediaJSON();  

    // Start Captive DNS redirection
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());



    // Set LED mode: solid (0), rainbow (1), etc.
    server.on("/led/onoff", HTTP_POST, [](AsyncWebServerRequest *request){},
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<64> doc;
        DeserializationError err = deserializeJson(doc, data);
        if (err) {
          request->send(400, "text/plain", "Invalid JSON");
          return;
        }
        bool enabled = doc["enabled"];
        RGB_SetMode(enabled ? 1 : 0);
        request->send(200, "text/plain", "LED toggled");
      }
    );

    // /led/rainbow - Start rainbow loop
    server.on("/led/rainbow", HTTP_POST, [](AsyncWebServerRequest *request) {
      RGB_SetMode(1);  // Rainbow loop
      request->send(200, "text/plain", "Rainbow mode activated");
    });

    // /led/color - Set solid color from JSON { color: "#rrggbb" }
    server.on("/led/color", HTTP_POST, [](AsyncWebServerRequest *request){},
      NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        StaticJsonDocument<64> doc;
        DeserializationError err = deserializeJson(doc, data);
        if (err) {
          request->send(400, "text/plain", "Invalid JSON");
          return;
        }

        const char* hex = doc["color"];
        if (!hex || strlen(hex) != 7 || hex[0] != '#') {
          request->send(400, "text/plain", "Invalid color format");
          return;
        }
        // Parse "#rrggbb"
        char rs[3] = { hex[1], hex[2], 0 };
        char gs[3] = { hex[3], hex[4], 0 };
        char bs[3] = { hex[5], hex[6], 0 };

        uint8_t r = strtol(rs, NULL, 16);
        uint8_t g = strtol(gs, NULL, 16);
        uint8_t b = strtol(bs, NULL, 16);
        RGB_SetColor(r, g, b);
        request->send(200, "text/plain", "Color set");
      }
    );

    server.on("/sdinfo", HTTP_GET, handleSDInfo);
    server.on("/generate-media", HTTP_POST, [](AsyncWebServerRequest *request){
      generateMediaJSON(); 
      request->send(200, "text/plain", "Done");
    });

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
    server.on("/dlna/desc.xml", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/xml", R"rawliteral(
        <?xml version="1.0"?>
        <root xmlns="urn:schemas-upnp-org:device-1-0">
          <specVersion>
            <major>1</major>
            <minor>0</minor>
          </specVersion>
          <device>
            <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
            <friendlyName>Nomad Media Server</friendlyName>
            <manufacturer>JCorp</manufacturer>
            <modelName>Nomad</modelName>
            <UDN>uuid:ESP32-DLNA-FAKE-1234</UDN>
          </device>
        </root>
      )rawliteral");
    });

    server.on("/dlna/contentdir.xml", HTTP_GET, [](AsyncWebServerRequest *request){
      String xml = "<?xml version=\"1.0\"?><ContentDirectory>";
      File root = SD_MMC.open("/Movies");
      if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
          if (!file.isDirectory()) {
            String name = file.name();
            xml += "<item>";
            xml += "<title>" + name + "</title>";
            xml += "<res protocolInfo=\"http-get:*:video/mp4:*\">";
            xml += "http://192.168.4.1/Movies/" + name + "</res>";
            xml += "</item>";
          }
          file = root.openNextFile();
        }
      }
      xml += "</ContentDirectory>";
      request->send(200, "text/xml", xml);
    });
    server.on("/listfiles", HTTP_GET, handleListFiles);


    // Static HTML routes
    server.serveStatic("/movies.html", SD_MMC, "/movies.html");
    server.serveStatic("/music.html", SD_MMC, "/music.html");
    server.serveStatic("/books.html", SD_MMC, "/books.html");
    server.serveStatic("/shows.html", SD_MMC, "/shows.html");
    server.serveStatic("/admin.html", SD_MMC, "/admin.html");

    // Serve root directory and default to index.html
    server.serveStatic("/", SD_MMC, "/").setDefaultFile("index.html");

    // Endpoint for video/audio streaming
    server.on("/media", HTTP_GET, handleRangeRequest);
    
    server.on("/upload-show", HTTP_POST,
      // 1) immediate responder:
      [](AsyncWebServerRequest *request) {
          request->send(200, "application/json", "{\"status\":\"Upload finished\"}");
      },
      // 2) upload handler:
      [](AsyncWebServerRequest *request, const String& filename, size_t index,
        uint8_t *data, size_t len, bool final) {
          static std::map<AsyncWebServerRequest*, File> showUploads;

          if (index == 0) {
              // Read subdir from query string:
              String subdir = "/";
              if (request->hasParam("subdir")) {
                  subdir = request->getParam("subdir")->value();
              }
              // Sanitize:
              if (!subdir.startsWith("/")) subdir = "/" + subdir;
              if (subdir.endsWith("/"))  subdir.remove(subdir.length() - 1);

              String fullPath = "/Shows" + subdir + "/" + filename;
              int slashPos = fullPath.lastIndexOf('/');
              if (slashPos != -1) {
                  SD_MMC.mkdir(fullPath.substring(0, slashPos));
              }

              Serial.println("[Upload] Starting upload to: " + fullPath);
              File f = SD_MMC.open(fullPath, FILE_WRITE);
              if (!f) {
                  Serial.println("[Upload] Failed to open file for writing");
                  return;
              }
              showUploads[request] = f;
          }

          if (showUploads.count(request)) {
              showUploads[request].write(data, len);
          }

          if (final && showUploads.count(request)) {
              Serial.println("[Upload] Upload complete for show episode.");
              showUploads[request].close();
              showUploads.erase(request);
          }
      }
    );
server.on("/mkdir", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasParam("dirname", true)) {
        request->send(400, "text/plain", "Missing dirname");
        return;
    }

    String dirName = request->getParam("dirname", true)->value();

    // Sanitize directory name: prevent absolute paths
    if (dirName.startsWith("/")) dirName = dirName.substring(1);

    // Build full path under /Shows
    String fullPath = "/Shows/" + dirName;

    if (SD_MMC.exists(fullPath)) {
        request->send(409, "text/plain", "Directory already exists");
        return;
    }

    if (SD_MMC.mkdir(fullPath)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "Failed to create directory");
    }
});

static std::map<AsyncWebServerRequest*, String> uploadPaths;
server.on("/rename", HTTP_POST, handleRename);
server.on("/delete", HTTP_POST, handleDelete);
    // Start the web server
    server.begin();
    Serial.println("Web Server started!");
}

// ==================== MAIN LOOP ====================

void loop() {
    dnsServer.processNextRequest();
    Timer_Loop();

    if (currentLEDMode == 1) {
        RGB_Lamp_Loop(2);
    }

    delay(5);
    updateClientCount();
}

void RGB_SetMode(uint8_t mode) {
    currentLEDMode = mode;

    if (mode == 0) {
        // Turn off LED immediately
        Set_Color(0, 0, 0);
    } else if (mode == 2) {
        Set_Color(solidG, solidR, solidB);
    }
}
