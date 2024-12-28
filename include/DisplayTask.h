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

namespace Soylent {
    class DisplayClass {
    public:
        DisplayClass(SPIClass& spi);
        void begin(Scheduler* scheduler);
        void end();
        void wipeDisplay();
        void printCenteredTag(uint16_t tagID);
        void powerOff(); 
        void hibernate();
        bool isInitialized();
        bool isBusy();

        // struct for passing parameters to async functions
        struct async_params
        {
            StatusRequest* srBusy;
            GxEPD2_3C<GxEPD2_154_Z90c, 200>* display;
            uint16_t* text_color;
            std::string* text_content;
        };

    private:
        void _initializeDisplayCallback();
        void _wipeDisplayCallback();
        static void _async_wipeDisplayTask(void* pvParameters);
        void _printCenteredTextCallback();
        static void _async_printCenteredTextTask(void* pvParameters);
        GxEPD2_3C<GxEPD2_154_Z90c, 200> _display;
        SPIClass* _spi;
        StatusRequest _srInitialized;
        StatusRequest _srBusy;
        Scheduler* _scheduler;
        struct async_params _async_params;
        uint16_t _text_color;
        std::string _text_content;
    };
} // namespace Soylent
