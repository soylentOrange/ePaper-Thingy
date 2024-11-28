// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#define TAG "WebServer"

extern const uint8_t logo_png_gz_start[] asm("_binary__pio_data_logo_png_gz_start");
extern const uint8_t logo_png_gz_end[] asm("_binary__pio_data_logo_png_gz_end");

WebServerClass::WebServerClass()
    : _webServer(TASK_IMMEDIATE, TASK_ONCE, std::bind(&WebServerClass::_webServerCallback, this), 
        NULL, false, NULL) {
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
    yield();
    webServer.end();
}

// Start the webserver
void WebServerClass::_webServerCallback() {
    LOGD(TAG, "WebServer callback...");

    // serve the logo
    webServer.on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOGD(TAG, "Serve logo...");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", logo_png_gz_start, logo_png_gz_end - logo_png_gz_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // clear persisted wifi config
    webServer.on("/clearwifi", HTTP_GET, [&](AsyncWebServerRequest* request) {
        LOGW(TAG, "Clearing WiFi configuration...");
        espConnect.clearConfiguration();
        LOGW(TAG, TAG, "Restarting!");
        ESPRestart.restartDelayed(500, ESPRestartClass::RestartFlag::resetWifi); // start task for delayed restart
        request->redirect("/");
    });

    webServer.onNotFound([](AsyncWebServerRequest* request) {
        LOGW(TAG, "Send 404 on request for %s", request->url().c_str());
        request->send(404);
    });

    webServer.begin();
} 

WebServerClass WebServer;
