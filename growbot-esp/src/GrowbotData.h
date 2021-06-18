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
struct GrowbotConfig {
    int exhaustFanPercent;
    int intakeFanPercent;
    int pumpPercent;
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

/* node red parsing code:
msg.payload = {
        timestamp: Date.now(),
        config: {
            exhaustFanPercent: msg.payload.readUInt8(0),
            intakeFanPercent: msg.payload.readUInt8(1),
            pumpPercent: msg.payload.readUInt8(2),
            controlWaterLevelCalibration : {
                fullCm: msg.payload.readUInt32LE(4),
                emptyCm: msg.payload.readUInt32LE(8)
                },
            bucketWaterLevelCalibration: [
                {
                    fullCm: msg.payload.readUInt32LE(12),
                    emptyCm: msg.payload.readUInt32LE(16)
                },
                {
                    fullCm: msg.payload.readUInt32LE(20),
                    emptyCm: msg.payload.readUInt32LE(24)
                    },
                {
                    fullCm: msg.payload.readUInt32LE(28),
                    emptyCm: msg.payload.readUInt32LE(32)
                },
                {
                    fullCm: msg.payload.readUInt32LE(36),
                    emptyCm: msg.payload.readUInt32LE(40)
                },
                ],
                exhaustFanCalibration: {
                    minOffset: msg.payload.readUInt8(44),
                    maxOffset: msg.payload.readUInt8(45)
                },
                intakeFanCalibration: {
                    minOffset: msg.payload.readUInt8(46),
                    maxOffset: msg.payload.readUInt8(47)
                },
                pumpCalibration: {
                    minOffset: msg.payload.readUInt8(48),
                    maxOffset: msg.payload.readUInt8(49)
                }
                
            },
            data: {
                exhaustInternal: {
                    temperatureC: msg.payload.readFloatLE(52),
                    humidity: msg.payload.readFloatLE(56)
                },
                intakeInternal: {
                    temperatureC: msg.payload.readFloatLE(60),
                    humidity: msg.payload.readFloatLE(64)
                },
                intakeExternal: {
                    temperatureC: msg.payload.readFloatLE(68),
                    humidity: msg.payload.readFloatLE(72)
                },
                ambientInternal: {
                    temperatureC: msg.payload.readFloatLE(76),
                    humidity: msg.payload.readFloatLE(80)
                },
                waterData: {
                    ph: msg.payload.readFloatLE(84),
                    tds: msg.payload.readFloatLE(88)
                },
                controlBucket: {
                    temperatureC: msg.payload.readFloatLE(92),
                    waterLevelPercent: msg.payload.readInt32LE(96)
                },
                buckets: [
                    {
                        temperatureC: msg.payload.readFloatLE(100),
                        waterLevelPercent: msg.payload.readInt32LE(104)
                    },
                    {
                        temperatureC: msg.payload.readFloatLE(108),
                        waterLevelPercent: msg.payload.readInt32LE(112)
                    },
                    {
                        temperatureC: msg.payload.readFloatLE(116),
                        waterLevelPercent: msg.payload.readInt32LE(120)
                    },
                    {
                        temperatureC: msg.payload.readFloatLE(124),
                        waterLevelPercent: msg.payload.readInt32LE(128)
                    }
                ]
            }
        }
    
return msg;

var offset = 0;

var readbyte = function(extraOffset) {
    var val = msg.payload.readUInt8(offset);
    offset++;
    if (extraOffset) {
        offset += extraOffset;
    }
    return val;
}
var readint = function() {
    var val = msg.payload.readUInt32LE(offset);
    offset += 4;
    return val;
}
var readfloat = function() {
    var val = msg.payload.readFloatLE(offset);
    offset += 4;
    return val;
}
msg.payload = {
        timestamp: Date.now(),
        current_mode: readbyte(),
        config: {
            exhaustFanPercent: readbyte(),
            intakeFanPercent: readbyte(),
            pumpPercent: readbyte(1),
            controlWaterLevelCalibration : {
                fullCm: readint(),
                emptyCm: readint()
                },
            bucketWaterLevelCalibration: [
                {
                    fullCm: readint(),
                    emptyCm: readint()
                },
                {
                    fullCm: readint(),
                    emptyCm: readint()
                    },
                {
                    fullCm: readint(),
                    emptyCm: readint()
                },
                {
                    fullCm: readint(),
                    emptyCm: readint()
                },
                ],
                exhaustFanCalibration: {
                    minOffset: readbyte(),
                    maxOffset: readbyte()
                },
                intakeFanCalibration: {
                    minOffset: readbyte(),
                    maxOffset: readbyte()
                },
                pumpCalibration: {
                    minOffset: readbyte(),
                    maxOffset: readbyte()
                }
                
            },
            data: {
                exhaustInternal: {
                    temperatureC: readfloat(),
                    humidity: readfloat()
                },
                intakeInternal: {
                    temperatureC: readfloat(),
                    humidity: readfloat()
                },
                intakeExternal: {
                    temperatureC: readfloat(),
                    humidity: readfloat()
                },
                ambientInternal: {
                    temperatureC: readfloat(),
                    humidity: readfloat()
                },
                waterData: {
                    ph: readfloat(),
                    tds: readfloat()
                },
                controlBucket: {
                    temperatureC: readfloat(),
                    waterLevelPercent: readfloat()
                },
                buckets: [
                    {
                        temperatureC: readfloat(),
                        waterLevelPercent: readfloat()
                    },
                    {
                        temperatureC: readfloat(),
                        waterLevelPercent: readfloat()
                    },
                    {
                        temperatureC: readfloat(),
                        waterLevelPercent: readfloat()
                    },
                    {
                        temperatureC: readfloat(),
                        waterLevelPercent: readfloat()
                    }
                ]
            }
        }
    
return msg;*/