#include <Arduino.h>
#include "DebugUtils.h"
#include "FlowValve.h"

#pragma once

class ComplexValve : public FlowValve {
    private:
        FlowValve* mainValve;
        FlowValve* secondaryValve;
        bool secondaryClosedWhenMainOn = true;
    public:
        ComplexValve(FlowValve* mainValve, FlowValve* secondaryValve, bool secondaryClosedWhenMainOn) {
            this->mainValve = mainValve;
            this->secondaryValve = secondaryValve;
            this->secondaryClosedWhenMainOn = secondaryClosedWhenMainOn;
        }

        void update() {
            this->mainValve->update();
            this->secondaryValve->update();
        }

        bool isOpen() {
            return mainValve->isOpen();
        }

        bool setOpen(bool open) {
            dbg.printf("ComplexValve: Setting main valve to %d\n", open);
            bool mainChange = mainValve->setOpen(open);
            dbg.printf("ComplexValve: Setting secondary valve to %d\n", (bool)(this->secondaryClosedWhenMainOn ^ open));
            bool secChange = secondaryValve->setOpen(this->secondaryClosedWhenMainOn ^ open);
            return (mainChange || secChange);
        }
};