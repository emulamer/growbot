#ifdef ARDUINO_ARCH_RP2040
#include <Arduino.h>
#include "libs/one_wire.h"
#include <DebugUtils.h>
#include "SensorBase.h"
#ifndef PICOONEWIRETEMPSENSOR_H
#define PICOONEWIRETEMPSENSOR_H
class WaterTempSensor : public SensorBase {
    public:
        virtual void init();
        virtual float read();
};

One_wire onewire(ONEWIRE_PIN);
bool onewireInitted = false;

class DallasWaterTempSensor : public SensorBase {
    private:
        rom_address_t addr;
        bool isOk = false;
    public:
        DallasWaterTempSensor(std::initializer_list<uint8_t> address) {
            assert(address.size() == 8);
            std::copy(address.begin(), address.end(), this->addr.rom);
        }
        void init() {
            if (!onewireInitted) {
                onewire.init();
                onewireInitted = true;
            }
            this->isOk = true;
            // this->sensor->setWaitForConversion(false);
            // this->sensor->begin();
            // this->isOk = this->sensor->isConnected(this->addr);
            // if (!this->isOk) {
            //     dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            // } else {
            //     if (!this->sensor->setResolution(this->addr, 12)) {
            //         dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to set sensor resolution!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //         this->isOk = false;
            //     }
            // }
            // this->sensor->requestTemperatures();
        }

        float read() {


            // if (!this->isOk) {
            //     dbg.printf("DallasWaterTempSensor: not on on read(), calling init() again...\n");
            //     this->init();
            // }
            // if (!this->isOk) {
            //     dbg.printf("Tried to read Dallas sensor at address %x %x %x %x %x %x %x %x but it is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //     return NAN;
            // }
            // if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
            //     dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //     this->isOk = false;
            //     return NAN;
            // }
            // delay(750);
            // for (byte i = 0; i < 3; i++) {
            //     float val = this->sensor->getTempC(this->addr);
            //     if (val == -127) {
            //         dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature, retry %d\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7], i);
            //     } else {
            //         return val;
            //     }
            // }
            return NAN;
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.isComplete = false;
            reading.isSuccessful = false;
            reading.values[0] = NAN;
            reading.readingCount = 1;
            reading.deferUntil = millis() + 1200;
            onewire.convert_temperature(this->addr, false, false);
            return reading;
            // if (!this->isOk) {
            //     this->init();
            // }
            // if (!this->isOk) {
            //     dbg.printf("Tried to read Dallas sensor at address %x %x %x %x %x %x %x %x but it is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //     reading.isComplete = true;
            //     reading.isSuccessful = false;
            //     reading.deferUntil = 0;
            // } else if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
            //     delay(100);
            //     //I don't know why this takes twice, I think maybe I got some cheap knockoffs
            //     if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
            //         dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x !\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //        // this->isOk = false;
            //         reading.isComplete = true;
            //         reading.isSuccessful = false;
            //         reading.deferUntil = 0;
            //     }
            // }
            // dbg.printf("about to check for dallas\n");
            // int count = onewire.find_and_count_devices_on_bus();
            // rom_address_t null_address{};
            // onewire.convert_temperature(null_address, true, true);
            // for (int i = 0; i < count; i++) {
            //     auto address = One_wire::get_address(i);
            //     dbg.printf("Address: %02x%02x%02x%02x%02x%02x%02x%02x\r\n", address.rom[0], address.rom[1], address.rom[2],
            //         address.rom[3], address.rom[4], address.rom[5], address.rom[6], address.rom[7]);
            //     dbg.printf("Temperature: %3.1foC\n", onewire.temperature(address));
            // }
            //  dbg.printf("done check for dallas\n");
            // return reading;
        }
        void finishRead(DeferredReading &reading) {
            reading.isComplete = true;
            float val = onewire.temperature(this->addr);
            if (val <= -127) {
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature\n", this->addr.rom[0], this->addr.rom[1], this->addr.rom[2], this->addr.rom[3], this->addr.rom[4], this->addr.rom[5], this->addr.rom[6], this->addr.rom[7]);
            } else {
                reading.isSuccessful = true;
                reading.values[0] = val;
            }
        }
};

#endif
#endif