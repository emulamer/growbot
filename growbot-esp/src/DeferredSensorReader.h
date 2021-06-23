#include <Arduino.h>
#include "DebugUtils.h"

#ifndef DEFERREDSENSORREADER_H
#define DEFERREDSENSORREADER_H

#define MAX_READING_COUNT 3

struct DeferredReading;
struct ReadingValue {
    bool isSuccessful;
    float dataPoint;
};
class DeferredSensorReader {
    public:
        virtual DeferredReading startRead() = 0;
        virtual void finishRead(DeferredReading &reading) = 0;
};
struct DeferredReading {
    DeferredSensorReader* sourceSensor;
    unsigned long deferUntil;
    bool isComplete;
    bool isSuccessful;
    byte readingCount;
    float values[MAX_READING_COUNT];
};





#endif