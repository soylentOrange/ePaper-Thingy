// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <Esp.h>
#define TAG "ESPRestart"

ESPRestartClass::ESPRestartClass()
    : _cleanupBeforeRestart(TASK_SECOND, TASK_ONCE, std::bind(&ESPRestartClass::_cleanupCallback, this), NULL, false)
    , _restart(TASK_SECOND, TASK_ONCE, std::bind(&ESPRestartClass::_restartCallback, this), NULL, false)
    , _restartFlag(RestartFlag::none)
    , _delayBeforeRestart(0) {
}

void ESPRestartClass::begin() {
    scheduler.addTask(_restart);
    scheduler.addTask(_cleanupBeforeRestart);
}

ESPRestartClass::RestartFlag ESPRestartClass::getState() {
    return _restartFlag;
}

void ESPRestartClass::restart(RestartFlag restartFlag = RestartFlag::none) {
    _restartFlag = restartFlag;
    _delayBeforeRestart = 0;
    _cleanupBeforeRestart.enable();
}

void ESPRestartClass::restartDelayed(unsigned long delayBeforeCleanup = 500, unsigned long delayBeforeRestart = 500, RestartFlag restartFlag = RestartFlag::none) {
    _restartFlag = restartFlag;
    _delayBeforeRestart = delayBeforeRestart;
    _cleanupBeforeRestart.enableDelayed(delayBeforeCleanup);
}

void ESPRestartClass::_cleanupCallback() {
    // Do some cleanup...
    WebSite.end();
    WebServer.end();
    ESPConnect.end();

    // ...and finally, the Restart-Task can be enabled subsequently
    _restart.enableDelayed(_delayBeforeRestart);
} 

// Just iniate the restart
void ESPRestartClass::_restartCallback() {
    esp_restart();
} 

ESPRestartClass ESPRestart;
