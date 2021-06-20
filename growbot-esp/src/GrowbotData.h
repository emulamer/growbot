#include <Arduino.h>
#include "WaterLevel.h"

#ifndef GROWBOTDATA_H
#define GROWBOTDATA_H

#define NUM_BUCKETS 4
struct WaterData {
    float ph;
    float tds;
}; 

struct BucketData {
    float temperatureC;
    float waterLevelPercent;
};

struct TempHumidity {
    float temperatureC;
    float humidity;
};
//these power offsets are in arbitrary units of 1-31 to offset which step starts the device and the last step that has any effect
//used because AC motors may not start below a certain point or may have no difference above a certain point
//by default, min = 1, max = 31
struct PowerControlCalibration {
    int minOffset;
    int maxOffset;
    int spinUpSec;
    int getRange() {
        return maxOffset - minOffset;
    }
};


#define EXHAUST_FAN_TOGGLE_BIT 0
#define INTAKE_FAN_TOGGLE_BIT 1
#define PUMP_TOGGLE_BIT 2
#define ROOM_FANS_TOGGLE_BIT 3
#define OVERHEAD_LIGHTS_TOGGLE_BIT 4
#define SIDE_LIGHTS_TOGGLE_BIT 5

struct GrowbotConfig {
    int exhaustFanPercent;
    int intakeFanPercent;
    int pumpPercent;
    int roomFansPercent;
    //these are bit flags for on/off for various things
    bool exhaustFanOn;
    bool intakeFanOn;
    bool pumpOn;
    bool roomFansOn;
    bool overheadLightsOn;
    bool sideLightsOn;
    WaterLevelCalibration controlWaterLevelCalibration;
    WaterLevelCalibration bucketWaterLevelCalibration[NUM_BUCKETS];
    PowerControlCalibration exhaustFanCalibration;
    PowerControlCalibration intakeFanCalibration;
    PowerControlCalibration pumpCalibration;
    int samplingIntervalMS;
};

struct GrowbotData {
    TempHumidity exhaustInternal;
    TempHumidity exhaustExternal;
    TempHumidity intakeInternal;
    TempHumidity intakeExternal;
    TempHumidity ambientInternal1;
    TempHumidity ambientInternal2;
    TempHumidity ambientInternal3;
    TempHumidity lights;
    float luxAmbient1;
    float luxAmbient2;
    float luxAmbient3;
    float luxAmbient4;
    WaterData waterData;
    BucketData controlBucket;
    BucketData buckets[NUM_BUCKETS];
};

struct GrowbotState {
    int current_mode;
    GrowbotConfig config;
    GrowbotData data;
};

#endif
