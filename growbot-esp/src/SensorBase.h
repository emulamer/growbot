#include <Arduino.h>
#include "DebugUtils.h"

#ifndef SensorBase_H
#define SensorBase_H

#define MAX_READING_COUNT 3

struct DeferredReading;
struct ReadingValue {
    bool isSuccessful;
    float dataPoint;
};
class SensorBase {
    public:
        virtual DeferredReading startRead() = 0;
        virtual void finishRead(DeferredReading &reading) = 0;
        virtual void init() = 0;
        virtual void preupdate(GrowbotData* data){};
        virtual void reconfigure(void* configAddr) {};
        virtual void handle() {};
};
struct DeferredReading {
    unsigned long deferUntil = 0;
    bool isComplete = false;
    bool isSuccessful = false;
    byte readingCount = 0;
    float values[MAX_READING_COUNT] = {0,0,0};
};





#endif