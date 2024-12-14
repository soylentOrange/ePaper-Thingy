// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define TAG "WebServer"

// gzipped logo
extern const uint8_t logo_start[] asm("_binary__pio_data_logo_captive_svg_gz_start");
extern const uint8_t logo_end[] asm("_binary__pio_data_logo_captive_svg_gz_end");

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

    // serve the logo
    webServer.on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve logo...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_start, logo_end - logo_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // clear persisted wifi config
    webServer.on("/clearwifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Clearing WiFi configuration...");
        espConnect.clearConfiguration();
        LOGW(TAG, TAG, "Restarting!");
        ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::resetWifi); // start task for delayed restart
        request->redirect("/");
    });

    // allow restart
    webServer.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Restarting!");
        ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::restartApp); // start task for delayed restart
        request->redirect("/");
    });

    // restart from safeboot-partition
    webServer.on("/safeboot", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Restart from safeboot...");
        const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_FACTORY, "safeboot");
        if (partition) {
            esp_ota_set_boot_partition(partition);
            ESPRestart.restartDelayed(500, 500, ESPRestartClass::RestartFlag::restartSafeboot); // start task for delayed restart
            request->redirect("/");
        } else {
            LOGW(TAG, "SafeBoot partition not found");
            request->redirect("/restart");
        }
    });

    // Set 404-handler only when the captive portal is not shown
    if (EventHandler.getState() != Mycila::ESPConnect::State::PORTAL_STARTED) {
        webServer.onNotFound([](AsyncWebServerRequest* request) {
            LOGW(TAG, "Send 404 on request for %s", request->url().c_str());
            request->send(404);
        });
    }    

    webServer.begin();

    _isRunning = true;
    LOGD(TAG, "WebServer started!");
} 

WebServerClass WebServer;
