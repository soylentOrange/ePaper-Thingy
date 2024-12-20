// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#define TAG "ESPConnect"

ESPConnectClass::ESPConnectClass()
    : _espConnect(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ESPConnectClass::_espConnectCallback, this), 
        NULL, false, NULL) {
}

void ESPConnectClass::begin() {
    // stop possibly running espConnect first
    espConnect.end();
    yield();  

    // get some info from espconnect's preferences
    Preferences preferences;
    preferences.begin("espconnect", true);
    std::string ssid;
    if (preferences.isKey("ssid"))
        ssid = preferences.getString("ssid").c_str();
    bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
    preferences.end();

    if (ssid.empty() || ap) {
        LOGI(TAG, "Trying to start captive portal in the background...");
    } else {
        LOGI(TAG, "Trying to connect to saved WiFi (%s) in the background...", ssid.c_str());
    }  

    // configure and begin espConnect
    espConnect.setAutoRestart(true);
    espConnect.setBlocking(false);
    espConnect.setCaptivePortalTimeout(ESPCONNECT_TIMEOUT_CAPTIVE_PORTAL);
    espConnect.setConnectTimeout(ESPCONNECT_TIMEOUT_CONNECT);    
    espConnect.begin(APP_NAME, CAPTIVE_PORTAL_SSID, CAPTIVE_PORTAL_PASSWORD);
    scheduler.addTask(_espConnect);
    _espConnect.enable();
}

void ESPConnectClass::end() {
    LOGD(TAG, "Stopping ESPConnect-Task...");
    _espConnect.disable();
    scheduler.deleteTask(_espConnect);
    espConnect.end();
} 

// Loop espConnect
void ESPConnectClass::_espConnectCallback() {
    espConnect.loop();
} 

ESPConnectClass ESPConnect;
