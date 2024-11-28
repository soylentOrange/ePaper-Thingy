// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#define TAG "Website"

extern const uint8_t favicon_svg_start[] asm("_binary_data_favicon_svg_start");
extern const uint8_t favicon_svg_end[] asm("_binary_data_favicon_svg_end");
extern const uint8_t touchicon_start[] asm("_binary__pio_data_apple_touch_icon_png_gz_start");
extern const uint8_t touchicon_end[] asm("_binary__pio_data_apple_touch_icon_png_gz_end");
extern const uint8_t favicon_png_start[] asm("_binary__pio_data_favicon_96x96_png_gz_start");
extern const uint8_t favicon_png_end[] asm("_binary__pio_data_favicon_96x96_png_gz_end");

WebsiteClass::WebsiteClass()
    : _website(TASK_SECOND, TASK_ONCE, NULL /*std::bind(&WebsiteClass::_websiteCallback, this)*/, 
        NULL, false, std::bind(&WebsiteClass::_websiteOnEnable, this)) {
}

void WebsiteClass::begin() {
    LOGD(TAG, "Enabling Website-Task...");
    scheduler.addTask(_website);
    _website.enable();
}

void WebsiteClass::end() {
    scheduler.deleteTask(_website);
}

// Check if Network is connected
bool WebsiteClass::_websiteOnEnable() {

    // Only enable when network is connected or AP (!Portal) is started
    yield();    
    Mycila::ESPConnect::State state = EventHandler.getState();
    if (state == Mycila::ESPConnect::State::NETWORK_CONNECTED || state == Mycila::ESPConnect::State::AP_STARTED) {

        // serve our home page here, yet only when the ESPConnect portal is not shown 
        webServer.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
            LOGD(TAG, "Serve...");
            time_t now = static_cast<time_t>(esp_timer_get_time() / 1000000);
            char strftime_buf[64];
            struct tm timeinfo;
            gmtime_r(&now, &timeinfo);
            sprintf(strftime_buf, "%d day%s ", timeinfo.tm_yday, timeinfo.tm_yday == 1 ? "" : "s"); 
            strftime(strftime_buf + strlen(strftime_buf), sizeof(strftime_buf) - strlen(strftime_buf), "%T", &timeinfo);
            auto *response = request->beginResponseStream("text/html; charset=utf-8");
            response->setCode(200);

            // write header
            response->print(
                "<!DOCTYPE html>"
                "<html lang=\"en\" >"
                "<head>"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,user-scalable=no\" />"
                    "<link rel=\"icon\" href=\"/favicon.svg\" />"
                    "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"/apple-touch-icon.png\" />"
                    "<link rel=\"icon\" type=\"image/png\" href=\"/favicon-96x96.png\" sizes=\"96x96\" />");
            response->printf(
                    "<title>%s</title> </head>", APP_NAME);
                    
            // write body
            response->print("<body><h2>Hello!</h2>");
            response->printf("<p>%s was build on %s at %s</p>", APP_NAME, __DATE__, __TIME__);
            response->printf("<p>Uptime: %s</p>",strftime_buf);
            switch (ESPRestart.getState()) {
                case ESPRestartClass::RestartFlag::resetWifi:
                    response->printf("<p>Wifi configuration cleared! Restarting now...</p>");
                    response->printf("<p>Look for AP %s</p>", CAPTIVE_PORTAL_SSID);
                    break;
                case ESPRestartClass::RestartFlag::resetAll:
                    response->printf("<p>Configuration cleared! Restarting now...</p>");
                    response->printf("<p>Look for AP %s</p>", CAPTIVE_PORTAL_SSID);
                    break;
                case ESPRestartClass::RestartFlag::restartPlain:
                    response->printf("<p>Restarting now...</p>");
                    break;
                case ESPRestartClass::RestartFlag::none:
                default:
                    break;           
            }

            // Write footer and send away...
            response->print("</body></html>");
            return request->send(response);
        }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

        // serve the favicon.svg
        webServer.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
            LOGD(TAG, "Serve favicon.svg...");
            AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", favicon_svg_start, favicon_svg_end - favicon_svg_start);
            response->addHeader("Cache-Control", "public, max-age=900");
            request->send(response);
        }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

        // serve the apple-touch-icon
        webServer.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* request) {
            LOGD(TAG, "Serve apple-touch-icon.png...");
            AsyncWebServerResponse* response = request->beginResponse(200, "image/png", touchicon_start, touchicon_end - touchicon_start);
            response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Cache-Control", "public, max-age=900");
            request->send(response);
        }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

        // serve the favicon.png
        webServer.on("/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest* request) {
            LOGD(TAG, "Serve favicon.png...");
            AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_png_start, favicon_png_end - favicon_png_start);
            response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Cache-Control", "public, max-age=900");
            request->send(response);
        }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

        // allow restart, yet only when the ESPConnect portal is not shown 
        webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
            LOGW(TAG, "Restarting!");
            ESPRestart.restartDelayed(500, ESPRestartClass::RestartFlag::restartPlain); // start task for delayed restart
            request->redirect("/");
        }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
        
        // // allow restart, yet only when the ESPConnect portal is not shown 
        // webServer.on("/taskinfo", HTTP_GET, [&](AsyncWebServerRequest* request) {
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
        // webServer.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
        //   LOGW(TAG, "Clearing WiFi configuration...");
        //   espConnect.clearConfiguration();
        //   LOGW(TAG, "Clearing app configuration...");
        //   Preferences preferences;
        //   preferences.begin(APP_NAME, false);
        //   preferences.clear();
        //   preferences.end();
        //   LOGW(TAG, TAG, "Restarting!");
        //   ESPRestart.restartDelayed(500, ESPRestartClass::RestartFlag::resetAll); // start task for delayed restart
        //   request->redirect("/");
        // }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

        return true;
    }

    // Only enable when network is connected
    return false;
} 

// Add Handlers to the webserver
void WebsiteClass::_websiteCallback() {
    LOGD(TAG, "Website-Task callback...");
    // For later use...
} 

WebsiteClass Website;
