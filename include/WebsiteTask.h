// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

class WebsiteClass {
public:
    WebsiteClass();
    void begin();
    void end();

private:
    Task _website;
    bool _websiteOnEnable();
    void _websiteCallback();
};

extern WebsiteClass Website;