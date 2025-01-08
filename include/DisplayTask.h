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
        void showImage(const char* imageName);
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
            std::string* image_name;
        };

        struct __attribute__ ((packed, aligned(1))) BITMAPFILEHEADER {
            uint16_t bType;             // identifier
            uint32_t bSize;             // filesize
            uint16_t bReserved1;        // reserved
            uint16_t bReserved2;        // reserved
            uint32_t bOffset;           // offset to start of pixel data
        };

        struct __attribute__ ((packed, aligned(1))) BITMAPINFOHEADER {
            uint32_t biInfoSize;        // size of this header
            int32_t biWidth;            // width
            int32_t biHeight;           // height
            uint16_t biPlanes;          // number of planes
            uint16_t biBitCount;        // bits per pixel
            uint32_t biCompression;     // compression type
            uint32_t biImageSize;       // size of the image, in bytes
            int32_t biXPelsPerMeter;    // horizontal resolution
            int32_t biYPelsPerMeter;    // vertical resolution
            uint32_t biClrUsed;         // number of colors used
            uint32_t biClrImportant;    // number of important colors
        };

    private:
        void _initializeDisplayCallback();
        void _wipeDisplayCallback();
        static void _async_wipeDisplayTask(void* pvParameters);
        void _printCenteredTextCallback();
        static void _async_printCenteredTextTask(void* pvParameters);
        void _showImageCallback();
        static void _async_showImageTask(void* pvParameters);
        GxEPD2_3C<GxEPD2_154_Z90c, 200> _display;
        SPIClass* _spi;
        StatusRequest _srInitialized;
        StatusRequest _srBusy;
        Scheduler* _scheduler;
        struct async_params _async_params;
        uint16_t _text_color;
        std::string _text_content;
        std::string _image_name;
    };
} // namespace Soylent
