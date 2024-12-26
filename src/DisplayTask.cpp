// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include "Fonts/FreeSans12pt7b.h"
#define TAG "Display"

DisplayClass::DisplayClass()
    : _initializeDisplay(TASK_IMMEDIATE, TASK_ONCE, [&] { _initializeDisplayCallback(); }, 
        NULL, false, NULL, NULL, false)
    , _display(GxEPD2_154_Z90c(DISPLAY_PIN_CS, DISPLAY_PIN_DC, DISPLAY_PIN_RST, DISPLAY_PIN_BUSY))
    , _hspi(HSPI)
    , _pScheduler(nullptr) {    
    _srBusy.setWaiting();
    _srInitialized.setWaiting();   
}

void DisplayClass::begin(Scheduler* scheduler) {
    LOGD(TAG, "Enabling Display-Task(s)...");
    yield();

    _srBusy.setWaiting();
    _srInitialized.setWaiting();
    _pScheduler = scheduler;    
    _pScheduler->addTask(_initializeDisplay);
    _initializeDisplay.enable();
}

void DisplayClass::end() {
    LOGD(TAG, "Hibernate Display...");
    if (_srInitialized.completed()) {
        _display.hibernate();
    }   
    _srBusy.setWaiting();
    _srInitialized.setWaiting(); 
    _initializeDisplay.disable();
    _pScheduler->deleteTask(_initializeDisplay);
    LOGD(TAG, "...done!");
}

// Initialize the display
void DisplayClass::_initializeDisplayCallback() {

    #ifdef LED_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    // set required display-pins as output
    pinMode(DISPLAY_PIN_CS, OUTPUT);
    digitalWrite(DISPLAY_PIN_CS, HIGH);
    pinMode(DISPLAY_PIN_DC, OUTPUT);
    pinMode(DISPLAY_PIN_RST, OUTPUT);

    // Initialize SPI and display
    _hspi.begin(DISPLAY_PIN_SPI_SCK, DISPLAY_PIN_SPI_MISO, DISPLAY_PIN_SPI_MOSI, DISPLAY_PIN_SPI_SS);
    _display.init(0, true, 2, false, _hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

    // a blank image (with text...)
    _display.setRotation(1);
    _display.setFont(&FreeSans12pt7b);
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        yield();
    } while (_display.nextPage());

    _display.powerOff();

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    LOGD(TAG, "Display initialized!");
    _srInitialized.signalComplete();
    _srBusy.signalComplete();
} 

bool DisplayClass::isInitialized() {
    return _srInitialized.completed();
} 

bool DisplayClass::isBusy() {
    return _srBusy.pending();
} 

void DisplayClass::powerOff() {
    if (_srInitialized.pending()) return;
    _display.powerOff();
} 

void DisplayClass::hibernate() {
    if (_srInitialized.pending()) return;
    _display.hibernate();
}

void DisplayClass::_wipeDisplayCallback() {
    // Flag display as busy
    _srBusy.setWaiting();

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    _display.clearScreen();
    _display.powerOff();

    LOGD(TAG, "Display wiped!");

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    _srBusy.signalComplete();
}

void DisplayClass::wipeDisplay() {

    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't wipe!");
        return;
    }

    LOGD(TAG, "Start wiping...");
    // create and run a task for wiping display...
    Task* wipeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _wipeDisplayCallback(); }, 
        _pScheduler, false, NULL, NULL, true);   
    wipeDisplayTask->enable();
    wipeDisplayTask->waitFor(&_srBusy);
}

void DisplayClass::_printCenteredTextCallback(std::string content, uint16_t c) {
    // Flag display as busy
    _srBusy.setWaiting();

    LOGD(TAG, "Do printing: %s", content.c_str());

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    // a blank image (with text...)
    _display.setTextColor(c);    
    int16_t tbx, tby; uint16_t tbw, tbh;
    _display.getTextBounds(content.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
    
    // center bounding box by transposition of origin:
    uint16_t x = ((_display.width() - tbw) / 2) - tbx;
    uint16_t y = ((_display.height() - tbh) / 2) - tby;
    _display.firstPage();
    do {
        _display.fillScreen(GxEPD_WHITE);
        _display.setCursor(x, y);
        _display.print(content.c_str());
        yield();
    } while (_display.nextPage());

    _display.powerOff();

    LOGD(TAG, "content printed");

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    _srBusy.signalComplete();
}

void DisplayClass::printCenteredTag(uint16_t tagID = NAME_TAG_BLACK) {

    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't print!");
        return;
    }

    std::string content(APP_NAME);
    uint16_t c = GxEPD_BLACK;

    switch (tagID) {
        case BLANK_TEXT:
            content = "";
            c = GxEPD_WHITE;
        case NAME_TAG_RED:
            c = GxEPD_RED;
            break;
        default:
            break;
    }

    LOGD(TAG, "Start printing: %s", content.c_str());
    // create and run a task for wiping display...
    Task* printTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&, content, c] { _printCenteredTextCallback(content, c); }, 
        _pScheduler, false, NULL, NULL, true);   
    printTask->enable();
    printTask->waitFor(&_srBusy);
}

DisplayClass Display;
