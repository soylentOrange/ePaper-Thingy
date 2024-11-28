// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2024 Robert Wendlandt
 */
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <MycilaESPConnect.h>

class EventHandlerClass {
public:
    EventHandlerClass();
    void begin();
    void end();
    Mycila::ESPConnect::State getState();

private:
    Task _eventHandler;
    void _eventHandlerCallback();
    Mycila::ESPConnect::State _previous;
    Mycila::ESPConnect::State _state;
};

extern EventHandlerClass EventHandler;
