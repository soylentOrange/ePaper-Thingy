// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

class ESPConnectClass {
public:
    ESPConnectClass();
    void begin(Scheduler* scheduler);
    void end();

private:
    Task _espConnect;
    void _espConnectCallback();
    Scheduler* _pScheduler;
};

extern ESPConnectClass ESPConnect;
