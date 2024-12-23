// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define TAG "WebServer"

// gzipped assets
extern const uint8_t logo_start[] asm("_binary__pio_assets_logo_captive_svg_gz_start");
extern const uint8_t logo_end[] asm("_binary__pio_assets_logo_captive_svg_gz_end");

WebServerClass::WebServerClass()
    : _webServer(TASK_IMMEDIATE, TASK_ONCE, std::bind(&WebServerClass::_webServerCallback, this), 
        NULL, false, NULL)
    , _isRunning(false) {
}

void WebServerClass::begin() {
    webServer.end();
    yield();
    scheduler.addTask(_webServer);
    _webServer.enable();
}

void WebServerClass::end() {
    LOGD(TAG, "Disabling WebServer-Task...");
    _webServer.disable();
    scheduler.deleteTask(_webServer);
    _isRunning = false;
    webServer.end();
    yield();
    LOGD(TAG, "WebServer-Task disabled!");
}

bool WebServerClass::isRunning() {
    return _isRunning;
}

// Start the webserver
void WebServerClass::_webServerCallback() {
    LOGD(TAG, "Starting WebServer...");

    // serve the logo (for captive portal)
    webServer.on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve captive logo...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_start, logo_end - logo_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return EventHandler.getState() == Mycila::ESPConnect::State::PORTAL_STARTED; });

    // clear persisted wifi config
    webServer.on("/clearwifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Clearing WiFi configuration...");
        espConnect.clearConfiguration();
        LOGW(TAG, TAG, "Restarting!");
        ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::resetWifi); // start task for delayed restart
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "WiFi credentials are gone! Restarting now...");
        request->send(response);
    });

    // do restart
    webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Restarting!");
        ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::restartApp); // start task for delayed restart
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting now...");
        request->send(response);
    });

    // restart from safeboot-partition
    webServer.on("/safeboot", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Restart from safeboot...");
        const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot");
        if (partition) {
            esp_ota_set_boot_partition(partition);
            ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::restartSafeboot); // start task for delayed restart
            AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting into SafeBoot now...");
            request->send(response);
        } else {
            LOGW(TAG, "SafeBoot partition not found");
            ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::restartSafeboot); // start task for delayed restart
            AsyncWebServerResponse* response = request->beginResponse(502, "text/plain", "SafeBoot partition not found!");
            request->send(response);
        }
    });

    // Set 404-handler only when the captive portal is not shown
    if (EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED) {
        LOGD(TAG, "Register 404 handler in WebServer");
        webServer.onNotFound([](AsyncWebServerRequest* request) {
            LOGW(TAG, "Send 404 on request for %s", request->url().c_str());
            request->send(404);
        });
    } else {
        LOGD(TAG, "Skip registering 404 handler in WebServer");
    }    

    webServer.begin();

    _isRunning = true;
    LOGD(TAG, "WebServer started!");
} 

WebServerClass WebServer;
