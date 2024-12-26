// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#define TAG "EventHandler"

EventHandlerClass::EventHandlerClass()
    : _eventHandler(TASK_SECOND, TASK_FOREVER, [&] { _eventHandlerCallback(); }, 
        NULL, false, NULL, NULL, false)
    , _previous(Mycila::ESPConnect::State::NETWORK_DISABLED)
    , _state(Mycila::ESPConnect::State::NETWORK_DISABLED)
    , _pScheduler(nullptr) {
}

void EventHandlerClass::begin(Scheduler* scheduler) {
    LOGD(TAG, "Enabling EventHandler-Task...");
    _state = espConnect.getState();
    _previous = _state;

    // Task handling
    _pScheduler = scheduler;
    _pScheduler->addTask(_eventHandler);
    _eventHandler.enable();
    LOGD(TAG, "EventHandler started!");
}

void EventHandlerClass::end() {
    LOGD(TAG, "Disabling EventHandler-Task...");
    _state = Mycila::ESPConnect::State::NETWORK_DISABLED;
    _previous = _state;
    _eventHandler.disable();
    _pScheduler->deleteTask(_eventHandler);
    LOGD(TAG, "EventHandler-Task disabled!");
}

Mycila::ESPConnect::State EventHandlerClass::getState() {
    return _state;
}

// Handle events from ESPConnect
void EventHandlerClass::_eventHandlerCallback() {

    // Do something on state change of espConnect
    _previous = _state;
    _state = espConnect.getState();
    if (_previous != _state) {
        switch (_state) {
            case Mycila::ESPConnect::State::NETWORK_CONNECTED:
                LOGI(TAG, "--> Connected to network...");
                yield();
                LOGI(TAG, "IPAddress: %s", espConnect.getIPAddress().toString().c_str());
                WebServer.begin(_pScheduler);
                yield();
                WebSite.begin(_pScheduler);
                break;

            case Mycila::ESPConnect::State::AP_STARTED:
                LOGI(TAG, "--> Created AP...");
                yield();
                LOGI(TAG, "SSID: %s", espConnect.getAccessPointSSID().c_str());
                LOGI(TAG, "IPAddress: %s", espConnect.getIPAddress().toString().c_str());
                WebServer.begin(_pScheduler);
                yield();
                WebSite.begin(_pScheduler);
                break;

            case Mycila::ESPConnect::State::PORTAL_STARTED:
                LOGI(TAG, "--> Started Captive Portal...");
                yield();
                LOGI(TAG, "SSID: %s", espConnect.getAccessPointSSID().c_str());
                LOGI(TAG, "IPAddress: %s", espConnect.getIPAddress().toString().c_str());
                WebServer.begin(_pScheduler);
                break;

            case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
                LOGI(TAG, "--> Disconnected from network...");
                WebSite.end();
                WebServer.end();
                break;

            case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
                LOGI(TAG, "--> Captive Portal has ended, auto-save the configuration...");
                auto config = espConnect.getConfig();
                LOGD(TAG, "ap: %d", config.apMode);
                LOGD(TAG, "wifiSSID: %s", config.wifiSSID.c_str());
                LOGD(TAG, "wifiPassword: %s", config.wifiPassword.c_str());
                break;
            }

            default:
                break;
        } /* switch (_state) */
    } /* if (_previous != _state) */
} 

EventHandlerClass EventHandler;
