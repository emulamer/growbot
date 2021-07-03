#include "NVStore.h"
#include "DebugUtils.h"
#include "hardware/sync.h"
//#include "mbed.h"
#include <FlashIAP.h>
#include <FlashIAPBlockDevice.h>
#ifndef PINVSTORE_H
#define PINVSTORE_H

using namespace mbed;

//lots of copy/paste stuff from https://www.arduino.cc/pro/tutorials/portenta-h7/por-ard-flash
// A helper struct for FlashIAP limits
struct FlashIAPLimits {
  size_t flash_size;
  uint32_t start_address;
  uint32_t available_size;
};

// Get the actual start address and available size for the FlashIAP Block Device
// considering the space already occupied by the sketch (firmware).

class PINVStore : public NVStore {
    private: 
        FlashIAPBlockDevice* blockDevice;
        FlashIAPLimits limits;
        uint32_t eraseBlockSize = 0;
        uint32_t programBlockSize = 0;
        FlashIAPLimits getFlashIAPLimits() {
            // Alignment lambdas
            auto align_down = [](uint64_t val, uint64_t size) {
                return (((val) / size)) * size;
            };
            auto align_up = [](uint32_t val, uint32_t size) {
                return (((val - 1) / size) + 1) * size;
            };

            size_t flash_size;
            uint32_t flash_start_address;
            uint32_t start_address;
            FlashIAP flash;

            auto result = flash.init();
            if (result != 0)
                return { };

            // Find the start of first sector after text area
            int sector_size = flash.get_sector_size(FLASHIAP_APP_ROM_END_ADDR);
            start_address = align_up(FLASHIAP_APP_ROM_END_ADDR, sector_size);
            flash_start_address = flash.get_flash_start();
            flash_size = flash.get_flash_size();

            result = flash.deinit();

            int available_size = flash_start_address + flash_size - start_address;
            if (available_size % (sector_size * 2)) {
                available_size = align_down(available_size, sector_size * 2);
            }

            return { flash_size, start_address, (uint32_t)available_size };
        }
    public:
        PINVStore() : NVStore() {
            limits = getFlashIAPLimits();
            blockDevice = new FlashIAPBlockDevice(limits.start_address, limits.available_size);
            blockDevice->init();
            programBlockSize = blockDevice->get_program_size();
            eraseBlockSize = blockDevice->get_erase_size();
        }

        void readConfig(GrowbotConfig* config) {
            uint8_t curVersion = 0;
            blockDevice->read(&curVersion, 0, 1);

            if (curVersion != CONFIG_VERSION) {
                dbg.printf("PINVStore: Read config version of %d doesn't match current version of %d, using defaults\n", curVersion, CONFIG_VERSION);
                readDefaultConfig(config);
                return;
            }
            dbg.printf("PINVStore: reading config from flash...\n");
            blockDevice->read((uint8_t*)config, 1, sizeof(GrowbotConfig));
            dbg.printf("PINVStore: config read\n");
        }
        void writeConfig(GrowbotConfig* config) {
            dbg.printf("PINVStore: Writing config to flash\n");
            
            const auto messageSize = sizeof(GrowbotConfig) + 1;
            const unsigned int requiredEraseBlocks = ceil(messageSize / (float)  eraseBlockSize);
            const unsigned int requiredProgramBlocks = ceil(messageSize / (float)  programBlockSize);
            const auto dataSize = requiredProgramBlocks * programBlockSize;  
            char* buffer  = (char*)malloc(dataSize);
            memset(buffer, 0, dataSize);
            buffer[0] = CONFIG_VERSION;
            memcpy(buffer + 1, config, sizeof(GrowbotConfig));
            blockDevice->erase(0, requiredEraseBlocks * eraseBlockSize);
            blockDevice->program(buffer, 0, dataSize);
            free(buffer);
            dbg.printf("PINVStore: Finished writing config to flash\n");
        }
};


#endif