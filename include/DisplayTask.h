// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <GxEPD2_3C.h>

#define BLANK_TEXT 0
#define NAME_TAG_BLACK 1
#define NAME_TAG_RED 2

class DisplayClass {
public:
    DisplayClass();
    void begin(Scheduler* scheduler);
    void end();
    void wipeDisplay();
    void printCenteredTag(uint16_t tagID);
    void powerOff(); 
    void hibernate();
    bool isInitialized();
    bool isBusy();

private:
    Task _initializeDisplay;
    void _initializeDisplayCallback();
    void _wipeDisplayCallback();
    void _printCenteredTextCallback(std::string content, uint16_t c);
    GxEPD2_3C<GxEPD2_154_Z90c, 200> _display;
    SPIClass _hspi;
    StatusRequest _srInitialized;
    StatusRequest _srBusy;
    Scheduler* _pScheduler;
};

extern DisplayClass Display;
