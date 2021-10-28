#include <Arduino.h>

#ifndef GROWBOTDATA_H
#define GROWBOTDATA_H

//note: data struct currently accounts for 4 buckets, when they're hooked up make this NUM_BUCKETS
#define DATA_BUCKET_COUNT 4

struct WaterData {
    float ph;
    float tds;
}; 

struct SwitchSchedule {
    int status; //0 = use schedule, 1 = force on, 2 = force off
    int turnOnGmtHourMin;
    int turnOffGmtHourMin;
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

enum ThermostatMode { Off, On, Thermostat };
struct ThermostatSetting {
    public:
        ThermostatMode mode;
};
struct FixedThermostatSetting : public ThermostatSetting {
    float minValue;
    float maxValue;
};


struct GrowbotConfig {
    int exhaustFanPercent;
    int intakeFanPercent;           
    //these are bit flags for on/off for various things
    bool exhaustFanOn;      
    bool intakeFanOn;
    bool pumpOn;
    //1 padding
    PowerControlCalibration exhaustFanCalibration;  //12
    PowerControlCalibration intakeFanCalibration;   //12
    int samplingIntervalMS;
    FixedThermostatSetting waterChillerThermostat; //12
    SwitchSchedule lightSchedule;   //12    
    SwitchSchedule roomFanSchedule; //12
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
    float luxAmbient5;
    WaterData waterData;
    BucketData controlBucket;
    float waterChillerStatus;
    bool lightsOn;
    bool roomFanOn;
    bool pumpOn;
    bool pumpEmergencyShutoff;
    BucketData buckets[DATA_BUCKET_COUNT];
};

struct GrowbotState {
    int current_mode;
    GrowbotConfig config;
    GrowbotData data;
};

#endif
