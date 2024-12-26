// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

class WebSiteClass {
public:
    WebSiteClass();
    void begin(Scheduler* scheduler);
    void end();

private:
    Task _website;
    void _websiteCallback();
    bool _fsMounted = false;
    int32_t _imageIdx;
    int32_t _imageCount;
    JsonDocument* _imagesJson;
    AsyncCallbackJsonWebHandler* _showImageHandler;
    Scheduler* _pScheduler;
};

extern WebSiteClass WebSite;
