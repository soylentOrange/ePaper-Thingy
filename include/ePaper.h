// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <MycilaESPConnect.h>
#include <FS.h>
#include <LittleFS.h>

#include <WebServerTask.h>
#include <WebSiteTask.h>
#include <ESPRestartTask.h>
#include <EventHandlerTask.h>
#include <ESPConnectTask.h>
#include <DisplayTask.h>

// in main.cpp
extern AsyncWebServer webServer;    
extern Scheduler scheduler;
extern Mycila::ESPConnect espConnect;

// Shorthands for Logging
#ifdef EPAPER_DEBUG
    #ifdef MYCILA_LOGGER_SUPPORT
        #include <MycilaLogger.h>
        extern Mycila::Logger logger;
        #define LOGD(tag, format, ...) logger.debug(tag, format, ##__VA_ARGS__)
        #define LOGI(tag, format, ...) logger.info(tag, format, ##__VA_ARGS__)
        #define LOGW(tag, format, ...) logger.warn(tag, format, ##__VA_ARGS__)
        #define LOGE(tag, format, ...) logger.error(tag, format, ##__VA_ARGS__)
    #else
        #define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
        #define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
        #define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
        #define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
    #endif
    #else
        #define LOGD(tag, format, ...)
        #define LOGI(tag, format, ...)
        #define LOGW(tag, format, ...)
        #define LOGE(tag, format, ...)
#endif
