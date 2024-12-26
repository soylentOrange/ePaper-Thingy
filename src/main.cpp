// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <TaskScheduler.h>
#include <SystemInfo.h>

// Create the WebServer, ESPConnect and Task-Scheduler here
AsyncWebServer webServer(HTTP_PORT);
Mycila::ESPConnect espConnect(webServer);
Scheduler scheduler;

void setup() {

    // Start Serial or USB-CDC
    #if !ARDUINO_USB_CDC_ON_BOOT
        Serial.begin(MONITOR_SPEED);
        // Only wait for serial interface to be set up when not using USB-CDC
        while (!Serial)
            yield();
    #else
        // USB-CDC doesn't need a baud rate
        Serial.begin();

        // Note: Enabling Debug via USB-CDC is handled via framework
    #endif

    // Get reason for restart
    LOGI(APP_NAME, "Reset reason: %s", SystemInfo.getResetReasonString().c_str());

    // Initialize the Scheduler
    scheduler.init();

    // Add Display-Task to Scheduler
    Display.begin(&scheduler);

    // Add Restart-Task to Scheduler
    ESPRestart.begin(&scheduler);

    // Add EventHandler to Scheduler
    // Will also spawn the WebServer and WebSite (when ESPConnect says so...)
    EventHandler.begin(&scheduler);

    // Add ESPConnect-Task to Scheduler
    ESPConnect.begin(&scheduler);
}

void loop() {
    scheduler.execute();
}
