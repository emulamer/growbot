#include <Arduino.h>

#ifndef CONFIG_H
#define CONFIG_H
// wifi/network config
#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"
#define MQTT_HOST "192.168.1.182"
#define MQTT_PORT 1883
#define MQTT_TOPIC "GROWBOT"
#define MQTT_CONFIG_TOPIC "GROWBOT_CONFIG"


#define PH_ENABLE_PIN 14
#define TDS_ENABLE_PIN 15

// version of the GrowbotConfig in eeprom, change if GrowbotConfic struct changes
#define CONFIG_VERSION 5

#define NUM_BUCKETS 0

// I2C
#define I2C_POWER_CTL_ADDR 0x27
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_2_SDA_PIN 18
#define I2C_2_SCL_PIN 19
#define I2C_FREQ 100000
#define I2C_MULTIPLEXER_ADDRESS 0x73
#define I2C_MULTIPLEXER2_ADDRESS 0x71

//Power control port config
#define POWER_EXHAUST_FAN_PORT 0
#define POWER_INTAKE_FAN_PORT 1
#define POWER_PUMP_PORT 2

//ultrasonic water level config

//fw update cmd (byte) 1
//total fw size (int)  1-4
//block sise    (int) 5-8
//num blocks    (int) 9-12
//<- ack
//block index   (int) 13-16
//block bytes   (byte[]) 17- (block size)
//<- ack
#define WL_CONTROL_BUCKET_MULTIPLEXER_PORT 0

#define WL_BUCKET_MULTIPLEXER_PORTS [ 1, 2, 3, 4 ]
 
//onewire submerged water temp sensors
#define ONEWIRE_PIN 21
//0x28, 0xEE, 0xB5, 0x0C, 0x23, 0x15, 0x00, 0xC8}
#define WT_CONTROL_BUCKET_ADDRESS {0x28,0xee,0xb5,0x0c,0x23,0x15,0x00,0xc8}//realone: { 0x28, 0x79, 0x6F, 0x76, 0xE0, 0x01, 0x3C, 0xD1 }
#define WT_BUCKET_ADDRESSES {0x28, 0x79, 0xFC, 0x76, 0xE0, 0x01, 0x3C, 0x28},\
                            {0, 1, 2, 3, 4, 5, 6, 7},\
                            {0, 1, 2, 3, 4, 5, 6, 7},\
                            {0, 1, 2, 3, 4, 5, 6, 7}

//sensor types and i2c multiplexor port assignments
#define INNER_EXHAUST_TYPE BME280Sensor
#define INNER_EXHAUST_MX_PORT 0

#define INNER_INTAKE_TYPE BME280Sensor
#define INNER_INTAKE_MX_PORT 1

#define OUTER_EXHAUST_TYPE SHTC3Sensor
#define OUTER_EXHAUST_MX_PORT 3

#define OUTER_INTAKE_TYPE SHTC3Sensor
#define OUTER_INTAKE_MX_PORT 2

#define INNER_AMBIENT_1_TYPE BME280Sensor
#define INNER_AMBIENT_1_MX_PORT 4

#define INNER_AMBIENT_2_TYPE BME280Sensor
#define INNER_AMBIENT_2_MX_PORT 5

// #define INNER_AMBIENT_3_TYPE SHTC3Sensor
// #define INNER_AMBIENT_3_MX_PORT 6

// #define LIGHTS_TEMP_TYPE BME280Sensor
// #define INNER_LIGHTS_MX_PORT 7

// lux sensor mutliplexer ports.  lux sensor is part of the GY-39 BME280 modules
#define LUX_SENSOR_1_MX_PORT INNER_EXHAUST_MX_PORT
#define LUX_SENSOR_2_MX_PORT INNER_INTAKE_MX_PORT
#define LUX_SENSOR_3_MX_PORT INNER_AMBIENT_1_MX_PORT
#define LUX_SENSOR_4_MX_PORT INNER_AMBIENT_2_MX_PORT
// #define LUX_SENSOR_5_MX_PORT INNER_LIGHTS_MX_PORT

#define PH_SENSOR_MX_PORT 0
#define TDS_SENSOR_MX_PORT 1
#define POWER_MX_PORT 3
#define CONTROL_WATER_LEVEL_MX_PORT 2


#endif