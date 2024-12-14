// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2023-2024 Mathieu Carbou, 2024 Robert Wendlandt
 */
#include <safeboot.h>

// The WebServer, obviously...
AsyncWebServer webServer(HTTP_PORT);
SafeBootOTAConnect otaConnect(webServer);

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
        Serial.enableReboot(true);
        Serial.setDebugOutput(true);
    #endif

    // Set next boot partition
    const esp_partition_t* partition = esp_partition_find_first(esp_partition_type_t::ESP_PARTITION_TYPE_APP, esp_partition_subtype_t::ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    if (partition) {
        esp_ota_set_boot_partition(partition);
    }    

    // Start SafeBootOTAConnect
    log_d("Blockingly trying to connect to saved WiFi or start portal...");
    otaConnect.begin(APP_NAME, SAFEBOOT_SSID, SAFEBOOT_AP_PASSWORD);

    // Now we should be connected to either the known Wifi or our own Soft-AP
    log_d("Setup finished!");
}

void loop() {
    otaConnect.loop();
}