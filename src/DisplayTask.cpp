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
    , _scheduler(nullptr)
    , _text_color(GxEPD_BLACK)
    , _text_content(APP_NAME) {    
    _srBusy.setWaiting();
    _srInitialized.setWaiting();   
}

void Soylent::DisplayClass::begin(Scheduler* scheduler) {
    yield();

    _srBusy.setWaiting();
    _srInitialized.setWaiting();
    _scheduler = scheduler;   
    // Set up the async task params
    _async_params.display = &_display;
    _async_params.srBusy = &_srBusy;
    _async_params.text_color = &_text_color;
    _async_params.text_content = &_text_content;
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
    _display.setRotation(1);
    _display.setFont(&FreeSans12pt7b);

    _srInitialized.signalComplete();
    LOGD(TAG, "...done!");

    // create and run a task for creating and running an async task for wiping the display...
    LOGD(TAG, "Wiping Display...");
    _wipeDisplayCallback();
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

// Wipe the display in async task
void Soylent::DisplayClass::_async_wipeDisplayTask(void* pvParameters) {
    auto params = static_cast<Soylent::DisplayClass::async_params*>(pvParameters);

    // display was flagged as busy externally...
    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    params->display->clearScreen();
    params->display->powerOff();

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    taskENTER_CRITICAL(&cs_spinlock);
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);    

    vTaskDelete(NULL);
}

void Soylent::DisplayClass::_wipeDisplayCallback() {
    _srBusy.setWaiting();
    xTaskCreate(_async_wipeDisplayTask, "wipeDisplayTask", configMINIMAL_STACK_SIZE, 
                (void*) &_async_params,
                tskIDLE_PRIORITY + 1, NULL);
    
    #ifdef DEBUG_ASYNC_TASK
        // Just for debugging...
        Task* report_async_wipeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, 
            [&] { LOGD(TAG, "...async wiping done!"); }, _scheduler, false, NULL, NULL, true);   
        report_async_wipeDisplayTask->enable();
        report_async_wipeDisplayTask->waitFor(&_srBusy);
    #endif
}

void Soylent::DisplayClass::wipeDisplay() {

    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't wipe!");
        return;
    }

    // create and run a task for creating and running an async task for wiping the display...
    // task is pending until the display is busy
    LOGD(TAG, "Start wiping...");
    Task* wipeDisplayTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _wipeDisplayCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    wipeDisplayTask->enable();
    wipeDisplayTask->waitFor(&_srBusy);
}

void Soylent::DisplayClass::_async_printCenteredTextTask(void* pvParameters) {
    auto params = static_cast<Soylent::DisplayClass::async_params*>(pvParameters);

    // display was flagged as busy externally...
    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    // a blank image (with text...)
    params->display->setTextColor(*params->text_color);    
    int16_t tbx, tby; uint16_t tbw, tbh;
    params->display->getTextBounds(params->text_content->c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
    
    // center bounding box by transposition of origin:
    uint16_t x = ((params->display->width() - tbw) / 2) - tbx;
    uint16_t y = ((params->display->height() - tbh) / 2) - tby;
    params->display->firstPage();
    do {
        params->display->fillScreen(GxEPD_WHITE);
        params->display->setCursor(x, y);
        params->display->print(params->text_content->c_str());
    } while (params->display->nextPage());

    params->display->powerOff();

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    taskENTER_CRITICAL(&cs_spinlock);
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);    

    vTaskDelete(NULL);
}

void Soylent::DisplayClass::_printCenteredTextCallback() {
    _srBusy.setWaiting();
    xTaskCreate(_async_printCenteredTextTask, "printCenteredTextTask", configMINIMAL_STACK_SIZE, 
                (void*) &_async_params,
                tskIDLE_PRIORITY + 1, NULL);
    
    #ifdef DEBUG_ASYNC_TASK
        // Just for debugging...
        Task* report_async_printCenteredTextTask = new Task(TASK_IMMEDIATE, TASK_ONCE, 
            [&] { LOGD(TAG, "...async printing done!"); }, _scheduler, false, NULL, NULL, true);   
        report_async_printCenteredTextTask->enable();
        report_async_printCenteredTextTask->waitFor(&_srBusy);
    #endif
}

void Soylent::DisplayClass::printCenteredTag(uint16_t tagID = NAME_TAG_BLACK) {

    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't print!");
        return;
    }

    // Store text-content and -color in the async_params struct
    _text_content = APP_NAME;
    _text_color = GxEPD_BLACK;

    switch (tagID) {
        case BLANK_TEXT:
            _text_content = "";
            _text_color = GxEPD_WHITE;
        case NAME_TAG_RED:
            _text_color = GxEPD_RED;
            break;
        default:
            break;
    }

    // create and run a task for creating and running an async task for printing on the display...
    // task is pending until the display is busy
    LOGD(TAG, "Start printing: %s", _text_content.c_str());
    Task* printTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _printCenteredTextCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    printTask->enable();
    printTask->waitFor(&_srBusy);
}
