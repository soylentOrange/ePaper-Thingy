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
        restartPlain,
        resetWifi,
        resetAll};
    void begin();
    void restart(RestartFlag flag);
    void restartDelayed(unsigned long delayBefore, RestartFlag flag);    
    RestartFlag getState();
private:
    Task _cleanupBeforeRestart;
    Task _restart;
    void _cleanupCallback();
    void _restartCallback();
    enum RestartFlag _restartFlag;
};

extern ESPRestartClass ESPRestart;
