// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou, 2024 Robert Wendlandt
 */
#include <SafeBootOTAConnect.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <functional>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <Update.h>

// gzipped favicons and update website
extern const uint8_t update_html_start[] asm("_binary__pio_data_update_html_gz_start");
extern const uint8_t update_html_end[] asm("_binary__pio_data_update_html_gz_end");
extern const uint8_t hotspot_detect_html_start[] asm("_binary__pio_data_hotspot_detect_html_gz_start");
extern const uint8_t hotspot_detect_html_end[] asm("_binary__pio_data_hotspot_detect_html_gz_end");
extern const uint8_t favicon_svg_start[] asm("_binary__pio_data_favicon_svg_gz_start");
extern const uint8_t favicon_svg_end[] asm("_binary__pio_data_favicon_svg_gz_end");
extern const uint8_t favicon_96_png_start[] asm("_binary__pio_data_favicon_96x96_png_gz_start");
extern const uint8_t favicon_96_png_end[] asm("_binary__pio_data_favicon_96x96_png_gz_end");
extern const uint8_t favicon_32_png_start[] asm("_binary__pio_data_favicon_32x32_png_gz_start");
extern const uint8_t favicon_32_png_end[] asm("_binary__pio_data_favicon_32x32_png_gz_end");
extern const uint8_t logo_start[] asm("_binary__pio_data_logo_safeboot_svg_gz_start");
extern const uint8_t logo_end[] asm("_binary__pio_data_logo_safeboot_svg_gz_end");

void SafeBootOTAConnect::begin(const char* hostname, const char* apSSID, const char* apPassword) {
    if (_state != SafeBootOTAConnect::State::NETWORK_DISABLED)
        return;

    // try to load the WiFi-configuration from the main app
    log_d("Loading config...");
    bool configOK = true;
    std::string ssid;
    std::string password;
    Preferences preferences;
    preferences.begin("espconnect", true);
    if (preferences.isKey("ssid"))
        ssid = preferences.getString("ssid").c_str();
    else
        configOK = false;  
    if (preferences.isKey("password"))
        password = preferences.getString("password").c_str();
    else
        configOK = false;
    bool ap = preferences.isKey("ap") ? preferences.getBool("ap", false) : false;
    preferences.end();

    if (configOK) {
        log_d("got config");
    } else {
        log_d("got no valid config");
        ap = true;
        ssid = SAFEBOOT_SSID;
        password = "";
    }

    begin(hostname, apSSID, apPassword, {ssid, password, ap});
}

void SafeBootOTAConnect::begin(const char* hostname, const char* apSSID, const char* apPassword, const SafeBootOTAConnect::Config& config) {
    if (_state != SafeBootOTAConnect::State::NETWORK_DISABLED)
        return;

    _hostname = hostname;
    _apSSID = apSSID;
    _apPassword = apPassword;
    _config = config; // copy values

    // Possibly disconnect from WiFi
    WiFi.mode(WIFI_MODE_NULL);
    
    _wifiEventListenerId = WiFi.onEvent(std::bind(&SafeBootOTAConnect::_onWiFiEvent, this, std::placeholders::_1));
    _state = SafeBootOTAConnect::State::NETWORK_ENABLED;

    log_d("Blockingly trying to connect to saved WiFi or pull up a softAP...");
    while (_state != SafeBootOTAConnect::State::OTA_UPDATER_STARTED) {
        loop();
        delay(100);
    }
}

void SafeBootOTAConnect::loop() {
    if (_dnsServer != nullptr)
        _dnsServer->processNextRequest();

    // check if we have to enter AP mode
    if (_state == SafeBootOTAConnect::State::NETWORK_ENABLED && _config.apMode) {
        log_d("Starting AP...");
        _startAP();
    }

    // otherwise, try to connect to WiFi
    if (_state == SafeBootOTAConnect::State::NETWORK_ENABLED && !_config.apMode) {
        log_d("Connecting to %s...", _config.wifiSSID.c_str());
        _startSTA();
    }

    // connection to WiFi times out ?
    if (_state == SafeBootOTAConnect::State::NETWORK_CONNECTING && _durationPassed(TIMEOUT_CONNECT)) {
        log_d("Timeout while connecting to  %s.", _config.wifiSSID.c_str());
        if (WiFi.getMode() != WIFI_MODE_NULL) {
            WiFi.config(static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000), static_cast<uint32_t>(0x00000000));
            WiFi.disconnect(true, true);
            log_d("Disconnecting from WiFi...");
        }
        _setState(SafeBootOTAConnect::State::NETWORK_TIMEOUT);
    }

    // start AP on connect timeout
    if (_state == SafeBootOTAConnect::State::NETWORK_TIMEOUT) {
        log_d("Starting AP after connection timeout...");
        _startAP();
    }

    // timeout OTA-Updater 
    if (_state == SafeBootOTAConnect::State::OTA_UPDATER_STARTED && _durationPassed(TIMEOUT_OTA_UPDATER)) {
        _setState(SafeBootOTAConnect::State::OTA_UPDATER_TIMEOUT);
        log_d("Timeout of OTA-Updater.");
    }

    // disconnected from network ? reconnect!
    if (_state == SafeBootOTAConnect::State::NETWORK_DISCONNECTED) {
        _setState(SafeBootOTAConnect::State::NETWORK_RECONNECTING);
        log_d("Disconnected from network.");
    }

    // Final state, we are either connected to a network or have created our own
    // The OTA-Services will be exposed 
    if (_state == SafeBootOTAConnect::State::AP_STARTED || _state == SafeBootOTAConnect::State::NETWORK_CONNECTED) {
        log_d("Starting OTA-Services...");
        _setState(SafeBootOTAConnect::State::OTA_UPDATER_STARTING);
        _enableOTAServices();
    }

    // Handle ArduinoOTA
    if (_state == SafeBootOTAConnect::State::OTA_UPDATER_STARTED && _arduinoOTAConfig.enableArduinoOTA) {
        ArduinoOTA.handle();
    }

    // Nothing has been uploaded...
    // Restart into the main app after timeout
    if (_state == SafeBootOTAConnect::State::OTA_UPDATER_TIMEOUT) {
        _restartDelayed();
    }
}

void SafeBootOTAConnect::end() {
    if (_state == SafeBootOTAConnect::State::NETWORK_DISABLED)
        return;

    _doCleanup();
}

void SafeBootOTAConnect::_setState(SafeBootOTAConnect::State state) {
    if (_state == state)
        return;

    const SafeBootOTAConnect::State previous = _state;
    _state = state;

    if (_callback != nullptr)
        _callback(previous, state);
}

void SafeBootOTAConnect::_restartDelayed(uint32_t msDelayBeforeCleanup, uint32_t msDelayBeforeRestart) {
    _doingRestart = true;
    _delayedTask.detach();
    _delayBeforeRestart = msDelayBeforeRestart;
    _delayedTask.once_ms(msDelayBeforeCleanup, [&]{_doCleanupAndRestart();});
}

void SafeBootOTAConnect::_doCleanup() {
    log_d("Doing cleanup...");

    // Resetting internal state variables and stop all services 
    WiFi.removeEvent(ARDUINO_EVENT_MAX);
    _state = SafeBootOTAConnect::State::NETWORK_DISABLED;
    _lastTime = -1;

    if(_arduinoOTAConfig.enableArduinoOTA) {
        ArduinoOTA.end();
    }

    if (_dnsServer != nullptr) {
        _dnsServer->stop();
        delete _dnsServer;
        _dnsServer = nullptr;
    }
    
    // forcefully shutting down WiFi
    _httpd = nullptr;
    WiFi.mode(WIFI_MODE_NULL);
}

void SafeBootOTAConnect::_doCleanupAndRestart() {
    _delayedTask.detach();
    _doCleanup();

    log_w("Restarting soon...");
    _delayedTask.once_ms(_delayBeforeRestart, esp_restart);
}

bool SafeBootOTAConnect::_durationPassed(uint32_t intervalSec) {
    if (_lastTime >= 0 && millis() - static_cast<uint32_t>(_lastTime) >= intervalSec * 1000) {
        _lastTime = -1;
        log_w("durationPassed triggered.");
        return true;
    }
    return false;
}

void SafeBootOTAConnect::_onArduinoOTAStarted() {
    log_d("ArduinoOTA has begun.");
    _lastTime = -1;
}

// Connect to WiFi
void SafeBootOTAConnect::_startSTA() {
    _setState(SafeBootOTAConnect::State::NETWORK_CONNECTING);

    log_d("Starting WiFi...");

    WiFi.setHostname(APP_NAME);
    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);

    if (_ipConfig.ip) {
        log_i("Set WiFi Static IP Configuration:");
        WiFi.config(_ipConfig.ip, _ipConfig.gateway, _ipConfig.subnet, _ipConfig.dns);
    }

    log_d("Connecting to SSID: %s...", _config.wifiSSID.c_str());
    WiFi.begin(_config.wifiSSID.c_str(), _config.wifiPassword.c_str());

    _lastTime = -1;
    log_d("WiFi started.");
}

void SafeBootOTAConnect::_startAP() {
    _setState(SafeBootOTAConnect::State::AP_STARTING);

    // Start AP
    WiFi.setHostname(APP_NAME);
    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.mode(WIFI_AP); 

    if (_apPassword.empty() || _apPassword.length() < 8) {
        // Disabling invalid Access Point password which must be at least 8 characters long when set
        WiFi.softAP(_apSSID.c_str(), "");
    } else
        WiFi.softAP(_apSSID.c_str(), _apPassword.c_str()); 

    if (_dnsServer == nullptr) {
        _dnsServer = new DNSServer();
        _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
        _dnsServer->start(53, "*", WiFi.softAPIP());
    }

    _lastTime = -1;
    log_d("Access Point started.");
}

void SafeBootOTAConnect::_enableOTAServices() {
    log_d("Start OTA-Services...");
    _setState(SafeBootOTAConnect::State::OTA_UPDATER_STARTING);

    // handle firmware upload
    _httpd->on("/update", HTTP_POST, [&](AsyncWebServerRequest *request) {
        _otaResultString = Update.hasError() ? Update.errorString() : "OTA successful!";
        _otaResultString += " Restarting now...";
        log_d("/update: %s", _otaResultString.c_str());
        _otaSuccess = !Update.hasError();
        _otaFinished = true;
        AsyncWebServerResponse* response = request->beginResponse(Update.hasError() ? 502 : 200, "text/plain", 
            _otaResultString.c_str());
        response->addHeader("Connection", "close");
        request->send(response);
        _restartDelayed(1000, 1000);
    },[&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){
            log_d("otaStarted...");
            _lastTime = -1;
            log_i("Receiving Update: %s, Size: %d", filename.c_str(), len);
            if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                log_e("Update error: %s", Update.errorString());
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                log_e("Update error: %s", Update.errorString());
            }
        }
        if (final) {
            if (Update.end(true)) {
                log_i("Update Success: %uB", index+len);
            } else {
                log_e("Update error: %s", Update.errorString());
            }
        }
    });

    // serve the favicon.svg
    _httpd->on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve favicon.svg");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", favicon_svg_start, favicon_svg_end - favicon_svg_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // serve the favicon.png
    _httpd->on("/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve favicon-96x96.png");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_96_png_start, favicon_96_png_end - favicon_96_png_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // serve the favicon.png
    _httpd->on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve favicon-32x32.png");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/png", favicon_32_png_start, favicon_32_png_end - favicon_32_png_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // serve the logo
    _httpd->on("/logo", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve logo");
        AsyncWebServerResponse* response = request->beginResponse(200, "image/svg+xml", logo_start, logo_end - logo_start);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=900");
        request->send(response);
    });

    // do restart
    _httpd->on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
        log_d("Restarting now...");
        _doingRestart = true;
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting now...");
        request->send(response);   
        _restartDelayed();     
    });

    // Serve restart info
    _httpd->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve restart info...");
        AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", "Restarting now...");
        response->addHeader("Refresh", "5");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return _doingRestart; });

    // Serve restart info
    _httpd->on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
        log_d("Serve restart info after OTA...");
        AsyncWebServerResponse* response = request->beginResponse(_otaSuccess ? 200 : 502, "text/plain", _otaResultString.c_str());
        response->addHeader("Refresh", "5");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return _otaFinished; });

    // Serve the OTA-Update website
    _httpd->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        log_d("Serve OTA-Update website");
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", update_html_start, update_html_end - update_html_start);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    }).setFilter([&](__unused AsyncWebServerRequest* request) { return (_state == SafeBootOTAConnect::State::OTA_UPDATER_STARTED) && !(_otaFinished || _doingRestart); });

    // serve apple CNA request
    _httpd->on("/hotspot-detect.html", [&](AsyncWebServerRequest* request) {
        if(++_hotspotDetectCounter > 2) {
            AsyncWebServerResponse* response = request->beginResponse(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
            request->send(response);
        } else {
            AsyncWebServerResponse* response = request->beginResponse(200, "text/html", hotspot_detect_html_start, hotspot_detect_html_end - hotspot_detect_html_start);
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        }
        log_d("Serve /hotspot-detect.html (count: %d)", _hotspotDetectCounter);
    });

    _httpd->onNotFound([](AsyncWebServerRequest* request) {
        log_d("Serve onNotFound: %s", request->url().c_str());
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html", hotspot_detect_html_start, hotspot_detect_html_end - hotspot_detect_html_start);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });

    // Start webserver
    _httpd->begin();

    // Start ArduinoOTA
    if (_arduinoOTAConfig.enableArduinoOTA) {
        ArduinoOTA.setHostname(APP_NAME);
        ArduinoOTA.setMdnsEnabled(false);   // We'll take care ourselves
        ArduinoOTA.setRebootOnSuccess(_arduinoOTAConfig.rebootOnSuccess);
        ArduinoOTA.setPort(_arduinoOTAConfig.port);
        if (!_arduinoOTAConfig.password.empty() && _arduinoOTAConfig.password.length() > 1) {
            ArduinoOTA.setPassword(_arduinoOTAConfig.password.c_str());
        }        
        ArduinoOTA.begin();
        MDNS.enableArduino(_arduinoOTAConfig.port);
        ArduinoOTA.onStart([&]{_onArduinoOTAStarted();});
    }     

    // Enable mdns for OTA-Updater
    MDNS.addService("http", "tcp", HTTP_PORT);
    
    // handle possible timeout
    _setState(SafeBootOTAConnect::State::OTA_UPDATER_STARTED);
    _lastTime = millis();

    log_d("OTA-Services started.");
}

// WiFi-event listener
void SafeBootOTAConnect::_onWiFiEvent(WiFiEvent_t event) {
    if (_state == SafeBootOTAConnect::State::NETWORK_DISABLED)
        return;

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            if (_state == SafeBootOTAConnect::State::NETWORK_CONNECTING || _state == SafeBootOTAConnect::State::NETWORK_RECONNECTING) {
                _lastTime = -1;
                MDNS.begin(APP_NAME);
                _setState(SafeBootOTAConnect::State::NETWORK_CONNECTED);
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
                WiFi.reconnect();
            } 
            if (_state == SafeBootOTAConnect::State::NETWORK_CONNECTED) {
                _setState(SafeBootOTAConnect::State::NETWORK_DISCONNECTED);
            }
            break;

        case ARDUINO_EVENT_WIFI_AP_START:        
            if (_state == SafeBootOTAConnect::State::AP_STARTING) {
                MDNS.begin(APP_NAME);
                _setState(SafeBootOTAConnect::State::AP_STARTED);
            }
            break;

        default:
            break;
    }
}