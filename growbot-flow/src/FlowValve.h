#include <Arduino.h>
#include "DebugUtils.h"

#pragma once

class FlowValve {
    public:
        virtual void update() { }
        virtual bool isOpen() = 0;
        //returns true if the state changed
        virtual bool setOpen(bool on) = 0;
};


