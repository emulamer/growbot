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

// version of the GrowbotConfig in eeprom, change if GrowbotConfic struct changes
#define CONFIG_VERSION 1

// I2C
#define I2C_POWER_CTL_ADDR 0x27
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ 100000
#define I2C_MULTIPLEXER_ADDRESS 0x71

//Power control port config
#define POWER_EXHAUST_FAN_PORT 0
#define POWER_INTAKE_FAN_PORT 1
#define POWER_PUMP_PORT 2

//ultrasonic water level config

#define WL_CONTROL_BUCKET_MULTIPLEXER_PORT 0
//the buckets aren't really set up
#define WL_BUCKET_ONE_MULTIPLEXER_PORT 1
#define WL_BUCKET_TWO_MULTIPLEXER_PORT 2
#define WL_BUCKET_THREE_MULTIPLEXER_PORT 3
#define WL_BUCKET_FOUR_MULTIPLEXER_PORT 4
 
//onewire submerged water temp sensors
#define ONEWIRE_PIN 15
#define WT_CONTROL_BUCKET_ADDRESS {0x28, 0x79, 0xFC, 0x76, 0xE0, 0x01, 0x3C, 0x28}
#define WT_BUCKET_1_ADDRESS {0, 1, 2, 3, 4, 5, 6, 7}
#define WT_BUCKET_2_ADDRESS {0, 1, 2, 3, 4, 5, 6, 7}
#define WT_BUCKET_3_ADDRESS {0, 1, 2, 3, 4, 5, 6, 7}
#define WT_BUCKET_4_ADDRESS {0, 1, 2, 3, 4, 5, 6, 7}

#endif