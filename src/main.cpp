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

        // // Enable Debug via USB-CDC
        // //#if !ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
        // #if ARDUINO_USB_CDC_ON_BOOT
        //     Serial.enableReboot(true);
        //     Serial.setDebugOutput(true);
        // #endif
    #endif

    // Get reason for restart
    LOGI(APP_NAME, "Reset reason: %s", SystemInfo.getResetReasonString().c_str());

    // Initialize the Scheduler
    scheduler.init();

    // Add Restart-Task to Scheduler
    ESPRestart.begin();

    // Add EventHandler to Scheduler
    EventHandler.begin();

    // Add ESPConnect-Task to Scheduler
    ESPConnect.begin();
}

void loop() {
    scheduler.execute();
}
