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
    , _text_content(APP_NAME)
    , _image_name("") {    
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
    _async_params.image_name = &_image_name;
    
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
    _display.setRotation(2);
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
        LOGW(TAG, "uninitialized, can't do it!");
        return;
    }

    // create and run a task for creating and running an async task for wiping the display...
    // task is pending until the display is not busy
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
        LOGW(TAG, "uninitialized, can't do it!");
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
    // task is pending until the display is not busy
    LOGD(TAG, "Start printing: %s", _text_content.c_str());
    Task* printTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _printCenteredTextCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    printTask->enable();
    printTask->waitFor(&_srBusy);
}

static unsigned char lookup[16] = {
0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

uint8_t reverse(uint8_t n) {
   // Reverse the top and bottom nibble then swap them.
   return (lookup[n&0b1111] << 4) | lookup[n>>4];
}

void reverse_pxcpy(uint8_t *__restrict dst, const uint8_t *__restrict src, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[n-1-i] = reverse(src[i]);
}

void Soylent::DisplayClass::_async_showImageTask(void* pvParameters) {
    auto params = static_cast<Soylent::DisplayClass::async_params*>(pvParameters);

    // display was flagged as busy externally...
    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, HIGH);
    #endif

    // open images to show
    std::vector<std::string> tokenized_imageName;
    Soylent::split_string(params->image_name->c_str(), tokenized_imageName, "/.");
    std::string file_name_red = "/" + tokenized_imageName[tokenized_imageName.size() - 2] + ".r.bmp";
    std::string file_name_black = "/" + tokenized_imageName[tokenized_imageName.size() - 2] + ".b.bmp";

    // open red file and get info
    File file_red = LittleFS.open(file_name_red.c_str(), "r");
    Soylent::DisplayClass::BITMAPFILEHEADER bmpFileHeader_red;
    Soylent::DisplayClass::BITMAPINFOHEADER bmpInfoHeader_red; 
    file_red.seek(0, fs::SeekMode::SeekSet);   
    file_red.read((uint8_t*) &bmpFileHeader_red, sizeof(bmpFileHeader_red));
    file_red.read((uint8_t*) &bmpInfoHeader_red, sizeof(bmpInfoHeader_red));

    // malloc a buffer for the bitmap part, which is oversized for the raw pixels
    // ...as the bitmap pixel data is aligned by 4 bytes
    uint8_t* buf_file = (uint8_t*) ps_malloc(bmpInfoHeader_red.biImageSize);

    // read the bitmap part from file
    file_red.seek(bmpFileHeader_red.bOffset, fs::SeekMode::SeekSet);   
    file_red.read(buf_file, bmpInfoHeader_red.biImageSize);
    file_red.close();

    // malloc a buffer for the red pixel data
    // ...and copy the raw pixel data
    auto bitmap_size = (bmpInfoHeader_red.biWidth * bmpInfoHeader_red.biHeight * bmpInfoHeader_red.biBitCount) / 8;
    auto row_width_pixel = (bmpInfoHeader_red.biWidth * bmpInfoHeader_red.biBitCount) / 8;
    auto row_width_bmp = (bmpInfoHeader_red.biWidth * bmpInfoHeader_red.biBitCount) / 8;
    row_width_bmp += (row_width_bmp % 4) == 0 ? 0 : 4 - (row_width_bmp % 4);
    uint8_t* bitmap_red = (uint8_t*) ps_malloc(bitmap_size);
    // copy pixels to match _display.setRotation(2);
    for (int32_t row = 0; row < bmpInfoHeader_red.biHeight; row++) {
        reverse_pxcpy(bitmap_red + row * row_width_pixel,
            buf_file + (bmpInfoHeader_red.biHeight - row) * row_width_bmp, 
            row_width_pixel);
    }

    // repeat for black image
    File file_black = LittleFS.open(file_name_black.c_str(), "r");
    Soylent::DisplayClass::BITMAPFILEHEADER bmpFileHeader_black;
    Soylent::DisplayClass::BITMAPINFOHEADER bmpInfoHeader_black; 
    file_black.seek(0, fs::SeekMode::SeekSet);   
    file_black.read((uint8_t*) &bmpFileHeader_black, sizeof(bmpFileHeader_black));
    file_black.read((uint8_t*) &bmpInfoHeader_black, sizeof(bmpInfoHeader_black));

    // check that both bitmaps are equal in format and are matching the panel
    if ((bmpInfoHeader_black.biImageSize == bmpInfoHeader_red.biImageSize) && 
        (bmpInfoHeader_red.biHeight == params->display->height()) && 
        (bmpInfoHeader_red.biWidth == params->display->width())) {
        // read the bitmap part from file
        file_black.seek(bmpFileHeader_black.bOffset, fs::SeekMode::SeekSet);   
        file_black.read(buf_file, bmpInfoHeader_black.biImageSize);
        file_black.close();

        // malloc a buffer for the red pixel data
        // ...and copy the raw pixel data
        auto bitmap_size = (bmpInfoHeader_black.biWidth * bmpInfoHeader_black.biHeight * bmpInfoHeader_black.biBitCount) / 8;
        auto row_width_pixel = (bmpInfoHeader_black.biWidth * bmpInfoHeader_black.biBitCount) / 8;
        auto row_width_bmp = (bmpInfoHeader_black.biWidth * bmpInfoHeader_black.biBitCount) / 8;
        row_width_bmp += (row_width_bmp % 4) == 0 ? 0 : 4 - (row_width_bmp % 4);
        uint8_t* bitmap_black = (uint8_t*) ps_malloc(bitmap_size);
        // copy pixels to match _display.setRotation(2);
        for (int32_t row = 0; row < bmpInfoHeader_black.biHeight; row++) {
            reverse_pxcpy(bitmap_black + row * row_width_pixel,
                buf_file + (bmpInfoHeader_black.biHeight - row) * row_width_bmp, 
                row_width_pixel);
        }

        // free the buffer
        free(buf_file);

        // draw the bitmaps
        // writeImage set to match _display.setRotation(2);
        params->display->writeScreenBuffer();
        params->display->writeImage(bitmap_black, 
                                    bitmap_red, 0, 0, 
                                    params->display->width(), 
                                    params->display->height(),
                                    false, true, false);
        params->display->refresh();
        params->display->powerOff();

        // free the bitmap buffers
        free(bitmap_red);
        free(bitmap_black);
    } else {
        // Do cleanup in case of image mismatch

        file_black.close();
        free(buf_file);
        free(bitmap_red);
    }    

    #ifdef LED_BUILTIN
        digitalWrite(LED_BUILTIN, LOW);
    #endif

    taskENTER_CRITICAL(&cs_spinlock);
    params->srBusy->signalComplete();
    taskEXIT_CRITICAL(&cs_spinlock);    

    vTaskDelete(NULL);
}

void Soylent::DisplayClass::_showImageCallback() {
    _srBusy.setWaiting();

    xTaskCreate(_async_showImageTask, "showImageTask", CONFIG_ASYNC_DISPLAY_STACK_SIZE, 
                (void*) &_async_params,
                tskIDLE_PRIORITY + 1, NULL);
    
    #ifdef DEBUG_ASYNC_TASK
        // Just for debugging...
        Task* report_async_showImageTask = new Task(TASK_IMMEDIATE, TASK_ONCE, 
            [&] { 
                LOGD(TAG, "...async imaging done!"); 
                LOGD(TAG, "%s", _image_name.c_str()); 
            }, _scheduler, false, NULL, NULL, true);   
        report_async_showImageTask->enable();
        report_async_showImageTask->waitFor(&_srBusy);
    #endif
}

void Soylent::DisplayClass::showImage(const char* imageName) {
    if (_srInitialized.pending()) {
        LOGW(TAG, "uninitialized, can't do it!");
        return;
    }

    // Store image name in the async_params struct
    _image_name.assign(imageName); 
    _async_params.image_name = &_image_name;
    LOGD(TAG, "got %s", _image_name.c_str());

    // create and run a task for creating and running an async task for imaging on the display...
    // task is pending until the display is not busy
    LOGD(TAG, "Start imaging: %s", _image_name.c_str());
    Task* showImageTask = new Task(TASK_IMMEDIATE, TASK_ONCE, [&] { _showImageCallback(); }, 
        _scheduler, false, NULL, NULL, true);   
    showImageTask->enable();
    showImageTask->waitFor(&_srBusy);
}
