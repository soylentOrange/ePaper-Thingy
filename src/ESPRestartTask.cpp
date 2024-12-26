// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <Esp.h>
#define TAG "ESPRestart"

ESPRestartClass::ESPRestartClass()
    : _cleanupBeforeRestart(TASK_IMMEDIATE, TASK_ONCE, [&] { _cleanupCallback(); }, 
        NULL, false, NULL, NULL, false)
    , _restart(TASK_IMMEDIATE, TASK_ONCE, [&] { _restartCallback(); }, 
        NULL, false, NULL, NULL, false)
    , _delayBeforeRestart(0)
    , _pScheduler(nullptr) {
}

void ESPRestartClass::begin(Scheduler* scheduler) {
    // Task handling
    _pScheduler = scheduler;
    _pScheduler->addTask(_restart);
    _pScheduler->addTask(_cleanupBeforeRestart);
}

void ESPRestartClass::restart() {
    _delayBeforeRestart = 0;
    _cleanupBeforeRestart.enable();
}

void ESPRestartClass::restartDelayed(unsigned long delayBeforeCleanup = 500, unsigned long delayBeforeRestart = 500) {
    _delayBeforeRestart = delayBeforeRestart;
    _cleanupBeforeRestart.enableDelayed(delayBeforeCleanup);
}

void ESPRestartClass::_cleanupCallback() {
    // Do some cleanup...
    WebSite.end();
    WebServer.end();
    ESPConnect.end();
    Display.end();

    // ...and finally, the Restart-Task can be enabled subsequently
    _restart.enableDelayed(_delayBeforeRestart);
} 

// Just iniate the restart
void ESPRestartClass::_restartCallback() {
    esp_restart();
} 

ESPRestartClass ESPRestart;
