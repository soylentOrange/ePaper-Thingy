// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>

namespace Soylent {
    class WebSiteClass {
    public:
        WebSiteClass(AsyncWebServer& webServer);
        void begin(Scheduler* scheduler);
        void end();

    private:
        void _webSiteCallback();
        bool _fsMounted = false;
        int32_t _imageIdx;
        int32_t _imageCount;
        JsonDocument* _imagesJson;
        AsyncCallbackJsonWebHandler* _showImageHandler;
        Scheduler* _scheduler;
        AsyncWebServer* _webServer;
    };
} // namespace Soylent
