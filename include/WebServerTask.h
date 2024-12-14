// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

class WebServerClass {
public:
    WebServerClass();
    void begin();
    void end();
    bool isRunning();

private:
    Task _webServer;
    void _webServerCallback();
    bool _isRunning;
};

extern WebServerClass WebServer;
