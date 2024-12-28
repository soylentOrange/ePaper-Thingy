// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#include <ePaper.h>
#include "Fonts/FreeSans12pt7b.h"
#define TAG "Display"

Soylent::DisplayClass::DisplayClass(SPIClass& spi)
    : _display(GxEPD2_154_Z90c(DISPLAY_PIN_CS, DISPLAY_PIN_DC, DISPLAY_PIN_RST, DISPLAY_PIN_BUSY))
    , _spi(&spi)
    , _scheduler(nullptr) {    
    _srBusy.setWaiting();
    _srInitialized.setWaiting();   
}

void Soylent::DisplayClass::begin(Scheduler* scheduler) {
    yield();

    _srBusy.setWaiting();
    _srInitialized.setWaiting();
    _scheduler = scheduler;   
    // create and run a task for initializing the display
    Task* initializeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _initializeDisplayCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    initializeDisplayTask->enable();

    LOGD(TAG, "Display is scheduled for start...");
}

void Soylent::DisplayClass::end() {
    LOGD(TAG, "Hibernate Display...");
    if (_srInitialized.completed()) {
        _display.hibernate();
    }   
    _srBusy.setWaiting();
    _srInitialized.setWaiting(); 
    LOGD(TAG, "...done!");
}

// Initialize the display
void Soylent::DisplayClass::_initializeDisplayCallback() {
    LOGD(TAG, "Initialize Display...");
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
    _spi->begin(DISPLAY_PIN_SPI_SCK, DISPLAY_PIN_SPI_MISO, DISPLAY_PIN_SPI_MOSI, DISPLAY_PIN_SPI_SS);
    _display.init(0, true, 2, false, *_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

    _srInitialized.signalComplete();
    LOGD(TAG, "...done!");

    // create and run a task for wiping the display...
    LOGD(TAG, "Wiping Display...");
    Task* wipeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _wipeDisplayCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    wipeDisplayTask->enable();
} 

bool Soylent::DisplayClass::isInitialized() {
    return _srInitialized.completed();
} 

bool Soylent::DisplayClass::isBusy() {
    return _srBusy.pending();
} 

void Soylent::DisplayClass::powerOff() {
    if (_srInitialized.pending()) return;
    _display.powerOff();
} 

void Soylent::DisplayClass::hibernate() {
    if (_srInitialized.pending()) return;
    _display.hibernate();
}

void Soylent::DisplayClass::_wipeDisplayCallback() {
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

void Soylent::DisplayClass::wipeDisplay() {

    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't wipe!");
        return;
    }

    // create and run a task for wiping the display...
    LOGD(TAG, "Start wiping...");
    Task* wipeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _wipeDisplayCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    wipeDisplayTask->enable();
    wipeDisplayTask->waitFor(&_srBusy);
}

void Soylent::DisplayClass::_printCenteredTextCallback(std::string content, uint16_t c) {
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

void Soylent::DisplayClass::printCenteredTag(uint16_t tagID = NAME_TAG_BLACK) {

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

    // create and run a task for wiping display...
    LOGD(TAG, "Start printing: %s", content.c_str());
    Task* printTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&, content, c] { _printCenteredTextCallback(content, c); }, 
        _scheduler, false, NULL, NULL, true);   
    printTask->enable();
    printTask->waitFor(&_srBusy);
}
