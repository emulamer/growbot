#include "NVStore.h"
#include "hardware/flash.h"
#include "DebugUtils.h"
#include "hardware/sync.h"
#include "mbed/hal/include/hal/flash_api.h"
#ifndef PINVSTORE_H
#define PINVSTORE_H

#define FLASH_TARGET_OFFSET PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE


class PINVStore : public NVStore {
    
    public:
        
        void readConfig(GrowbotConfig* config) {
            uint8_t curVersion = *((uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET));
            if (curVersion != CONFIG_VERSION) {
                dbg.printf("Read config version of %d doesn't match current version of %d, using defaults\n", curVersion, CONFIG_VERSION);
                readDefaultConfig(config);
                return;
            }
            memcpy(config, (void*)(XIP_BASE + FLASH_TARGET_OFFSET + 1), sizeof(GrowbotConfig));
        }
        void writeConfig(GrowbotConfig* config) {
            flash_t flash;        
            if (!flash_init(&flash)) {
                dbg.println("flash failed init");
                return;
            }
            int configSize = (sizeof(GrowbotConfig) + 1)/FLASH_PAGE_SIZE;
            if ((sizeof(GrowbotConfig) + 1) % FLASH_PAGE_SIZE != 0) {
                configSize++;
            }
            configSize *= FLASH_PAGE_SIZE;
            dbg.printf("config total size is %d\n", configSize);
            uint8_t sector[configSize];
            memset(sector, 0, configSize);
            sector[0] = CONFIG_VERSION;
            memcpy(sector+1, config, sizeof(GrowbotConfig));
            uint32_t ints = save_and_disable_interrupts();
            flash_erase_sector(&flash, FLASH_TARGET_OFFSET);
            //flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
            for (int i = 0; i < FLASH_SECTOR_SIZE; i+= FLASH_PAGE_SIZE) {
                flash_program_page(&flash, FLASH_TARGET_OFFSET + i, sector+i, FLASH_PAGE_SIZE);
                //flash_range_program(FLASH_TARGET_OFFSET + i, sector + i, FLASH_PAGE_SIZE);
            }            
            restore_interrupts(ints);
        }
};

#endif