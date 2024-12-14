// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <Preferences.h>
#include <SafeBootOTAConnect.h>
#include <string>
#include <esp_partition.h>
#include <esp_ota_ops.h>

// in main.cpp
extern AsyncWebServer webServer; 
extern SafeBootOTAConnect otaConnect;
