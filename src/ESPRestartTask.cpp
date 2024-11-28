// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <Esp.h>
#define TAG "ESPRestart"

ESPRestartClass::ESPRestartClass()
    : _cleanupBeforeRestart(TASK_IMMEDIATE, TASK_ONCE, std::bind(&ESPRestartClass::_cleanupCallback, this), NULL, false)
    , _restart(TASK_IMMEDIATE, TASK_ONCE, std::bind(&ESPRestartClass::_restartCallback, this), NULL, false)
    , _restartFlag(RestartFlag::none) {
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
    _cleanupBeforeRestart.enable();
}

void ESPRestartClass::restartDelayed(unsigned long delayBefore, RestartFlag restartFlag = RestartFlag::none) {
    _restartFlag = restartFlag;
    _cleanupBeforeRestart.enableDelayed(delayBefore);
}

void ESPRestartClass::_cleanupCallback() {
    // Do some cleanup...
    Website.end();
    WebServer.end();
    ESPConnect.end();
    // ..., yield...
    yield();

    // ...and finally, the Restart-Task can be enabled subsequently
    _restart.enable();
} 

// Just iniate the restart
void ESPRestartClass::_restartCallback() {
    ESP.restart();
} 

ESPRestartClass ESPRestart;
