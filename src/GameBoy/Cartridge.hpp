#pragma once

#include <cstdint>

constexpr uint16_t ROM_BANK_SIZE = 16 * 1024;
constexpr uint16_t ERAM_BANK_SIZE = 8 * 1024;

enum class MBC { MBC0, MBC1, MBC2, MBC3, MBC5 };

enum class CartRegion { ROM0, ROM1, RAM };

struct Cartridge {
    MBC Mapper;

    int romBanks;
    int ramBanks;

    uint8_t(*rom)[ROM_BANK_SIZE];
    uint8_t(*ram)[ERAM_BANK_SIZE];

    bool hasBatteryBackup;

    union {
        struct {
            bool isRamEnabled;
            uint8_t currentRomBank5;
            uint8_t currentRomBank2;
            uint8_t memoryMode;
        } MBC1;

        struct {
            bool isRamEnabled;
            union {
                uint16_t currentRomBank;
                struct {
                    uint8_t currentRomBankLow;
                    uint8_t currentRomBankHigh;
                };
            };
            uint8_t cur_ram_bank;
        } MBC5;
    };

};

Cartridge* createCartridge(const char* fileName);
void destroyCartridge(Cartridge* Cart);

uint8_t readFromCartridge(Cartridge* Cart, uint16_t Address, CartRegion Region);
void writeToCartridge(Cartridge* Cart, uint16_t Address, CartRegion Region, uint8_t Data);
