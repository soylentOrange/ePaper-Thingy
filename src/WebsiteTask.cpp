// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#define TAG "website"

// gzipped assets
extern const uint8_t logo_thingy_start[] asm("_binary__pio_assets_logo_thingy_svg_gz_start");
extern const uint8_t logo_thingy_end[] asm("_binary__pio_assets_logo_thingy_svg_gz_end");
extern const uint8_t touchicon_start[] asm("_binary__pio_assets_apple_touch_icon_png_gz_start");
extern const uint8_t touchicon_end[] asm("_binary__pio_assets_apple_touch_icon_png_gz_end");
extern const uint8_t favicon_svg_start[] asm("_binary__pio_assets_favicon_svg_gz_start");
extern const uint8_t favicon_svg_end[] asm("_binary__pio_assets_favicon_svg_gz_end");
extern const uint8_t favicon_96_png_start[] asm("_binary__pio_assets_favicon_96x96_png_gz_start");
extern const uint8_t favicon_96_png_end[] asm("_binary__pio_assets_favicon_96x96_png_gz_end");
extern const uint8_t favicon_32_png_start[] asm("_binary__pio_assets_favicon_32x32_png_gz_start");
extern const uint8_t favicon_32_png_end[] asm("_binary__pio_assets_favicon_32x32_png_gz_end");
extern const uint8_t thingy_html_start[] asm("_binary__pio_assets_thingy_html_gz_start");
extern const uint8_t thingy_html_end[] asm("_binary__pio_assets_thingy_html_gz_end");

Soylent::WebSiteClass::WebSiteClass(AsyncWebServer& webServer)
    : _imageIdx(0)
    , _showImageHandler(nullptr)
    , _imagesJson(nullptr)
    , _imageCount(1)
    , _scheduler(nullptr)
    , _webServer(&webServer) {
}

void Soylent::WebSiteClass::begin(Scheduler* scheduler) {
    LOGD(TAG, "Enabling WebSite-Task...");

    // Task handling
    _scheduler = scheduler;
    // create and run a task for setting up the website
    Task* webSiteTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _webSiteCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    webSiteTask->enable();
    webSiteTask->waitFor(WebServer.getStatusRequest());
}

void Soylent::WebSiteClass::end() {
    LittleFS.end();
    if (_showImageHandler != nullptr) {
        delete _showImageHandler;
        _showImageHandler = nullptr;
    }
    if (_imagesJson != nullptr) {
        delete _imagesJson;
        _imagesJson = nullptr;
    }
}

// Add Handlers to the webserver
void Soylent::WebSiteClass::_webSiteCallback() {
    LOGD(TAG, "Starting WebSite...");

    // try reading /images.json
    LOGD(TAG, "Reading images.json...");
    File file = LittleFS.open("/images.json", "r");
    if (!file || file.isDirectory()) { 
        LOGE(TAG, "An Error has occurred while reading images.json!"); 
    } else {
        std::string fileContent;
        _imagesJson = new JsonDocument();
        while (file.available()) {
            char intRead = file.read();
            fileContent += intRead;
        }            
        file.close();
        deserializeJson(*_imagesJson, fileContent);
        _imagesJson->shrinkToFit();
        if (!_imagesJson->as<JsonObject>()["images"].is<JsonArray>()) {
            LOGE(TAG, "An Error has occurred while parsing images.json for images!");
        } else {
            _imageCount =  _imagesJson->as<JsonObject>()["images"].as<JsonArray>().size();
            LOGI(TAG, "images.json seems fine! (%d images)", _imageCount);
            _fsMounted = true;
        }
    }

    // Prepare handler for showing images
    _showImageHandler = new AsyncCallbackJsonWebHandler("/display/showimage");
    _showImageHandler->setMethod(HTTP_PUT);
    _showImageHandler->setFilter([&](__unused AsyncWebServerRequest* request) { return (EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED &&  _fsMounted); });
    _showImageHandler->onRequest([&] (AsyncWebServerRequest* request, JsonVariant& json ) {
        LOGD(TAG, "Serve /display/showimage");
        auto img_idx = json.as<JsonObject>()["img_idx"].as<int32_t>();
        auto img_idx_max = _imagesJson->as<JsonObject>()["images"].as<JsonArray>().size();
        LOGD(TAG, "Got img_idx: %d", img_idx);
        if (Display.isBusy() || !Display.isInitialized()) {
            LOGW(TAG, "Not available right now");
            request->send(503, "text/plain", "Display not available right now");
        } else if (img_idx < 0 || img_idx > img_idx_max) {
            LOGW(TAG, "img_idx out of bounds");
            request->send(418, "text/plain", "img_idx out of bounds");
        } else {
            switch (img_idx) {
                case 0: 
                    // 0 is hardcoded to wiping
                    // not part of the images.json but embedded in the html-code
                    LOGI(TAG, "I want to wipe!");                  
                    Display.wipeDisplay();
                    _imageIdx = img_idx;                  
                    break;
                case 1:    
                    // 1 & 2 are hardcoded to printing text
                    // a svg is present only for display on the website  
                    // the epaper is written with text                   
                    LOGI(TAG, "I want to print in black!");                  
                    Display.printCenteredTag(NAME_TAG_BLACK);
                    _imageIdx = img_idx;                  
                    break;
                case 2: 
                    // 1 & 2 are hardcoded to printing text
                    // a svg is present only for display on the website  
                    // the epaper is written with text    
                    LOGI(TAG, "I want to print in red!");                 
                    Display.printCenteredTag(NAME_TAG_RED);
                    _imageIdx = img_idx;                  
                    break;
                default: {
                    // show an image from littleFS
                    LOGI(TAG, "I want to show an image!"); 
                    auto img_name = _imagesJson->as<JsonObject>()["images"].as<JsonArray>()[img_idx-1].as<JsonObject>()["src"];
                    Display.showImage(img_name);  
                    _imageIdx = img_idx;
                }                    
            }
            
            request->send(200, "text/plain", "OK");           
        }        
    }); 

    // Register handler for showing images
    _webServer->addHandler(_showImageHandler);

    // serve from File System
    _webServer->serveStatic("/images/", LittleFS, "/").setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });

    // serve from File System
    _webServer->serveStatic("/images.json", LittleFS, "/images.json", "no-store").setFilter([&](__unused AsyncWebServerRequest* request) { return _fsMounted; });
    
    // serve request for display state
    _webServer->on("/display/state", HTTP_GET, [&](AsyncWebServerRequest* request) {
        // LOGD(TAG, "Serve /display/state");
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        if (Display.isBusy()) {
            root["state"] = "in_progress";
        } else if (!Display.isInitialized()) {
            root["state"] = "not_initialized";
        } else {
            root["state"] = "idle";
        }
        
        root["img_idx"] = _imageIdx;
        serializeJson(root, *response);
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; }); 

    // serve the logo (for main page)
    _webServer->on("/thingy_logo", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve thingy logo...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_thingy_start, logo_thingy_end - logo_thingy_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // serve the favicon.svg
    _webServer->on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve favicon.svg...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", favicon_svg_start, favicon_svg_end - favicon_svg_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // serve the apple-touch-icon
    _webServer->on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve apple-touch-icon.png...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", touchicon_start, touchicon_end - touchicon_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // serve the favicon.png
    _webServer->on("/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve favicon-96x96.png...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_96_png_start, favicon_96_png_end - favicon_96_png_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // serve the favicon.png
    _webServer->on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve favicon-32x32.png...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_32_png_start, favicon_32_png_end - favicon_32_png_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // serve our home page here, yet only when the ESPConnect portal is not shown 
    _webServer->on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
        // LOGD(TAG, "Serve...");
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", thingy_html_start, thingy_html_end - thingy_html_start);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

    // // serve our home page here, yet only when the ESPConnect portal is not shown 
    // _webServer->on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
    //     LOGD(TAG, "Serve...");
    //     time_t now = static_cast<time_t>(esp_timer_get_time() / 1000000);
    //     char strftime_buf[64];
    //     struct tm timeinfo;
    //     gmtime_r(&now, &timeinfo);
    //     sprintf(strftime_buf, "%d day%s ", timeinfo.tm_yday, timeinfo.tm_yday == 1 ? "" : "s"); 
    //     strftime(strftime_buf + strlen(strftime_buf), sizeof(strftime_buf) - strlen(strftime_buf), "%T", &timeinfo);
    //     auto *response = request->beginResponseStream("text/html; charset=utf-8");
    //     response->setCode(200);

    //     // write header
    //     response->print(
    //         "<!DOCTYPE html>"
    //         "<html lang=\"en\" >"
    //         "<head>"
    //             "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\" />"
    //             "<link rel=\"icon\" href=\"/favicon.svg\" />"
    //             "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"/apple-touch-icon.png\" />"
    //             "<link rel=\"icon\" type=\"image/png\" href=\"/favicon-96x96.png\" sizes=\"96x96\" />"
    //             "<link rel=\"icon\" type=\"image/png\" href=\"/favicon-32x32.png\" sizes=\"32x32\" />");
    //     response->printf(
    //             "<title>%s</title> </head>", APP_NAME);
                
    //     // write body
    //     response->print("<body><h2>Hello!</h2>");
    //     response->printf("<p>%s was build on %s at %s</p>", APP_NAME, __DATE__, __TIME__);
    //     response->printf("<p>Uptime: %s</p>",strftime_buf);
    //     switch (ESPRestart.getState()) {
    //         case ESPRestartClass::RestartFlag::resetWifi:
    //             response->printf("<p>Wifi configuration cleared! Restarting now...</p>");
    //             response->printf("<p>Look for AP %s</p>", CAPTIVE_PORTAL_SSID);
    //             response->addHeader("Refresh", "5");
    //             break;
    //         case ESPRestartClass::RestartFlag::resetAll:
    //             response->printf("<p>Configuration cleared! Restarting now...</p>");
    //             response->printf("<p>Look for AP %s</p>", CAPTIVE_PORTAL_SSID);
    //             response->addHeader("Refresh", "5");
    //             break;
    //         case ESPRestartClass::RestartFlag::restartApp:
    //             response->printf("<p>Restarting now...</p>");
    //             response->addHeader("Refresh", "5");
    //             break;
    //         case ESPRestartClass::RestartFlag::restartSafeboot:
    //             response->printf("<p>Restarting into safeboot now...</p>");
    //             response->addHeader("Refresh", "5");
    //             break;
    //         case ESPRestartClass::RestartFlag::none:
    //         default:
    //             break;           
    //     }

    //     // Write footer and send away...
    //     response->print("</body></html>");
    //     return request->send(response);
    // }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
    
    // // allow restart, yet only when the ESPConnect portal is not shown 
    // _webServer->on("/taskinfo", HTTP_GET, [&](AsyncWebServerRequest* request) {
    //     LOGD(TAG, "/taskinfo!");
    //     uxTaskGetSystemState()
    //     LOGD(TAG, "tasks: %d", uxTaskGetNumberOfTasks());            
    //     static std::array<char const*, 12> constexpr task_names = {
    //         "IDLE0", "IDLE1", "wifi", "tiT", "loopTask", "async_tcp", "mqttclient",
    //         "HUAWEI_CAN_0", "PM:SDM", "PM:HTTP+JSON", "PM:SML", "PM:HTTP+SML"
    //     };
    //     for (char const* task_name : task_names) {
    //         TaskHandle_t const handle = xTaskGetHandle(task_name);
    //         if (!handle) { continue; }
    //         LOGD(TAG, "task: %s - %d - %d", task_name, uxTaskGetStackHighWaterMark(handle), uxTaskPriorityGet(handle));
    //     }

    //     request->redirect("/");
    // }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
    
    // // clear all persisted config
    // _webServer->on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    //   LOGW(TAG, "Clearing WiFi configuration...");
    //   espConnect.clearConfiguration();
    //   LOGW(TAG, "Clearing app configuration...");
    //   Preferences preferences;
    //   preferences.begin(APP_NAME, false);
    //   preferences.clear();
    //   preferences.end();
    //   LOGW(TAG, TAG, "Restarting!");
    //   ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::resetAll); // start task for delayed restart
    //   request->redirect("/");
    // }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
    
    LOGD(TAG, "...done!");
} 
