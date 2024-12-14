// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

class ESPRestartClass {
public:
    ESPRestartClass();
    enum class RestartFlag {
        none,
        restartApp,
        restartSafeboot,
        resetWifi,
        resetAll};
    void begin();
    void restart(RestartFlag flag);
    void restartDelayed(unsigned long delayBeforeCleanup, unsigned long delayBeforeRestart, RestartFlag flag);    
    RestartFlag getState();
private:
    Task _cleanupBeforeRestart;
    Task _restart;
    void _cleanupCallback();
    void _restartCallback();
    enum RestartFlag _restartFlag;
    unsigned long _delayBeforeRestart;
};

extern ESPRestartClass ESPRestart;
