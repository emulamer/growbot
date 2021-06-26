#include "EEPROMAnything.h"
#include <EEPROM.h>
#include "NVStore.h"

#ifndef EEPROMNVSTORE_H
#define EEPROMNVSTORE_H


class EEPROMNVStore : public NVStore {
    public: 
        EEPROMNVStore() {
            EEPROM.begin(512);
        }
        void readConfig(GrowbotConfig* config) {
            byte version = EEPROM.read(0);
            if (version != CONFIG_VERSION) {
                dbg.printf("Config stored in eeprom is old version %d, resetting to default\n", version);
                this->readDefaultConfig(config);
            } else {
                EEPROM_readAnything<GrowbotConfig>(1, config);
            }
        }
        void writeConfig(GrowbotConfig* config) {
            EEPROM.write(0, (byte)CONFIG_VERSION);
            EEPROM_writeAnything<GrowbotConfig>(1, config);
            EEPROM.commit();
        }
};
      

#endif