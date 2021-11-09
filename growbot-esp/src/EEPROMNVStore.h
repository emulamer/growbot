#include "EEPROMAnything.h"
#include <EEPROM.h>
#include "NVStore.h"

#ifndef EEPROMNVSTORE_H
#define EEPROMNVSTORE_H


class EEPROMNVStore : public NVStore {
    public: 
        EEPROMNVStore() {
            
        }

        void init() {
            EEPROM.begin(512);
        }
        void readConfig(GrowbotConfig* config) {
            byte version = EEPROM.read(0);
            if (version != CONFIG_VERSION) {
                dbg.wprintf("Config stored in eeprom is old version %d, resetting to default\n", version);
                this->readDefaultConfig(config);
            } else {
                EEPROM_readAnything<GrowbotConfig>(1, *config);
            }
        }
        void writeConfig(GrowbotConfig &config) {
            dbg.dprintln("writing config to eeprom");
            EEPROM.write(0, (byte)CONFIG_VERSION);
            EEPROM_writeAnything<GrowbotConfig>(1, config);
            if (!EEPROM.commit()) {
                dbg.eprintln("EEPROM commit failed!");
            } else {
                dbg.dprintln("done");
                byte v = EEPROM.read(0);
                if (v != CONFIG_VERSION) {
                    dbg.wprintf("Didn't read correct version from eeprom!\n");
                }
            }
        }
};
      

#endif