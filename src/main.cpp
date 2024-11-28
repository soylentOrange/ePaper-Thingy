// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include <TaskScheduler.h>

// Create the WebServer, ESPConnect and Task-Scheduler here
AsyncWebServer webServer(HTTP_PORT);
Mycila::ESPConnect espConnect(webServer);
Scheduler scheduler;

void setup() {
  Serial.begin(MONITOR_SPEED);
  #if !ARDUINO_USB_CDC_ON_BOOT
    // Only wait for serial interface to be set up when not using USB-CDC
    while (!Serial)
        yield();
  #endif

  // // Enable debug output and reboot over USB-CDC on arduino-2
  // #if ESP_ARDUINO_VERSION_MAJOR < 3
  //   #if !ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  //     Serial.enableReboot(true);
  //     Serial.setDebugOutput(true);
  //   #endif
  // #endif

  // Initialize the Scheduler
  scheduler.init();

  // Add Restart-Task to Scheduler
  ESPRestart.begin();

  // Add EventHandler to Scheduler
  EventHandler.begin();

  // Add ESPConnect-Task to Scheduler
  ESPConnect.begin();  

  // Add ArduinoOTA-Task to Scheduler
  #ifdef ARDUINO_OTA
    // ArduinoOTA.begin();
  #endif
}

void loop() {
  scheduler.execute();
}
