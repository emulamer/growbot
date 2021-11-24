#include <Arduino.h>
#include "DebugUtils.h"
#include "FlowValve.h"
#include "SolenoidValve.h"

#pragma once

class ToggleValve : public FlowValve {
    private:
        byte openPin;
        byte closePin;
        int msToOpen = -1;
        int msToClose = -1;
        FlowValve* openValve;
        FlowValve* closeValve;
        bool isOpened = true;
        void setState() {
            dbg.printf("ToggleValve: toggling to %d\n", isOpened);
            if (isOpened) {
                closeValve->setOpen(false);
                openValve->setOpen(true);
            } else {
                openValve->setOpen(false);
                closeValve->setOpen(true);
            }
        }
    public:
        ToggleValve(byte openPin, byte closePin, int msToOpen = -1, int msToClose = -1) {
            this->openPin= openPin;
            this->closePin= closePin;
            this->msToOpen = msToOpen;
            this->msToClose = msToClose;
            if (msToOpen < 1) {
                this->openValve = new SolenoidValve(openPin, 1, 255);
            } else {
                this->openValve = new SolenoidValve(openPin, msToOpen, 0);
            }
            if (msToClose < 1) {
                this->closeValve = new SolenoidValve(closePin, 1, 255);
            } else {
                this->closeValve = new SolenoidValve(closePin, msToClose, 0);
            }
            setState();
        }
        void update() {
            this->openValve->update();
            this->closeValve->update();
        }

        bool setOpen(bool open) {
            if (isOpened == open) {
                dbg.dprintf("ToggleValve: Requested state %d is already the current state\n", open);
                return false;
            }
            isOpened = open;
            setState();
            return true;
        }

        bool isOpen() {
            return isOpened;
        }
              
  };