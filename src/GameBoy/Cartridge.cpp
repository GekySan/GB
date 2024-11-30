#include "Cartridge.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

struct HandleDeleter {
    void operator()(HANDLE handle) const {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

using unique_handle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleDeleter>;

std::string generateSAVFilename(const std::string& fileName) {
    std::filesystem::path path(fileName);
    return path.replace_extension(".sav").string();
}

Cartridge* createCartridge(const char* fileName)
{
    std::ifstream file(fileName, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Impossible d'ouvrir le fichier: " << fileName << std::endl;
        return nullptr;
    }

    file.seekg(0x0147, std::ios::beg);
    std::vector<uint8_t> headerData(3);
    file.read(reinterpret_cast<char*>(headerData.data()), headerData.size());
    if (file.gcount() < static_cast<std::streamsize>(headerData.size())) {
        std::cerr << "Erreur de lecture de l'en-tête du fichier." << std::endl;
        return nullptr;
    }

    file.seekg(0, std::ios::beg);

    MBC Mapper;
    uint8_t MapperCode = headerData[0];
    if (MapperCode == 0 || MapperCode == 0x08 || MapperCode == 0x09)
        Mapper = MBC::MBC0;
    else if (MapperCode >= 0x01 && MapperCode <= 0x03)
        Mapper = MBC::MBC1;
    else if (MapperCode == 0x05 || MapperCode == 0x06)
        Mapper = MBC::MBC2;
    else if (MapperCode >= 0x0f && MapperCode <= 0x13)
        Mapper = MBC::MBC3;
    else if (MapperCode >= 0x19 && MapperCode <= 0x1e)
        Mapper = MBC::MBC5;
    else {
        std::cerr << "Mapper non supporté: " << static_cast<int>(MapperCode) << std::endl;
        return nullptr;
    }

    bool Battery = false;
    switch (MapperCode) {
    case 0x03:
    case 0x06:
    case 0x09:
    case 0x0d:
    case 0x0f:
    case 0x10:
    case 0x13:
    case 0x1b:
    case 0x1e:
    case 0x22:
        Battery = true;
        break;
    default:
        Battery = false;
        break;
    }

    int romBanks = (headerData[1] <= 8) ? (2 << headerData[1]) : -1;
    if (romBanks == -1) {
        std::cerr << "Nombre de banques ROM invalide: " << static_cast<int>(headerData[1]) << std::endl;
        return nullptr;
    }

    int ramBanks;
    switch (headerData[2]) {
    case 0:
    case 1:
        ramBanks = 0;
        break;
    case 2:
        ramBanks = 1;
        break;
    case 3:
        ramBanks = 4;
        break;
    case 4:
        ramBanks = 16;
        break;
    case 5:
        ramBanks = 8;
        break;
    default:
        std::cerr << "Nombre de banques RAM invalide: " << static_cast<int>(headerData[2]) << std::endl;
        return nullptr;
    }

    auto Cart = std::make_unique<Cartridge>();
    Cart->Mapper = Mapper;
    Cart->hasBatteryBackup = Battery;
    Cart->romBanks = romBanks;
    Cart->ramBanks = ramBanks;

    std::vector<uint8_t> rom_Data(romBanks * ROM_BANK_SIZE);
    file.read(reinterpret_cast<char*>(rom_Data.data()), rom_Data.size());
    if (file.gcount() < static_cast<std::streamsize>(rom_Data.size())) {
        std::cerr << "Erreur de lecture des données ROM." << std::endl;
        return nullptr;
    }

    uint8_t(*rom)[ROM_BANK_SIZE] = reinterpret_cast<uint8_t(*)[ROM_BANK_SIZE]>(
        std::malloc(romBanks * ROM_BANK_SIZE));
    if (!rom) {
        std::cerr << "Échec de l'allocation de la mémoire ROM." << std::endl;
        return nullptr;
    }
    std::memcpy(rom, rom_Data.data(), rom_Data.size());
    Cart->rom = rom;

    if (ramBanks > 0) {
        if (Battery) {
            std::string sav_filename = generateSAVFilename(fileName);
            unique_handle hSavFile(CreateFileA(
                sav_filename.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (hSavFile.get() == INVALID_HANDLE_VALUE) {
                std::cerr << "Erreur lors de l'ouverture du fichier .sav: " << sav_filename << std::endl;
                std::free(Cart->rom);
                return nullptr;
            }

            LARGE_INTEGER liSize;
            liSize.QuadPart = static_cast<LONGLONG>(ramBanks * ERAM_BANK_SIZE);
            if (!SetFilePointerEx(hSavFile.get(), liSize, nullptr, FILE_BEGIN) ||
                !SetEndOfFile(hSavFile.get())) {
                std::cerr << "Erreur lors de la définition de la taille du fichier .sav." << std::endl;
                return nullptr;
            }

            unique_handle hMap(CreateFileMappingA(
                hSavFile.get(),
                nullptr,
                PAGE_READWRITE,
                0,
                0,
                nullptr));

            if (!hMap) {
                std::cerr << "Erreur lors de la création du mapping de fichier." << std::endl;
                return nullptr;
            }

            LPVOID mappedView = MapViewOfFile(
                hMap.get(),
                FILE_MAP_READ | FILE_MAP_WRITE,
                0,
                0,
                ramBanks * ERAM_BANK_SIZE);

            if (!mappedView) {
                std::cerr << "Erreur lors du mappage de la vue du fichier .sav." << std::endl;
                return nullptr;
            }

            Cart->ram = reinterpret_cast<uint8_t(*)[ERAM_BANK_SIZE]>(mappedView);
        }
        else {

            uint8_t(*ram)[ERAM_BANK_SIZE] = reinterpret_cast<uint8_t(*)[ERAM_BANK_SIZE]>(
                std::calloc(ramBanks, ERAM_BANK_SIZE));
            if (!ram) {
                std::cerr << "Échec de l'allocation de la mémoire RAM." << std::endl;
                std::free(Cart->rom);
                return nullptr;
            }
            Cart->ram = ram;
        }
    }

    return Cart.release();

}

void destroyCartridge(Cartridge* Cart)
{
    if (!Cart)
        return;

    if (Cart->rom) {
        std::free(Cart->rom);
    }

    if (Cart->ram) {
        if (Cart->hasBatteryBackup) {
            UnmapViewOfFile(Cart->ram);
        }
        else {
            std::free(Cart->ram);
        }
    }

    std::free(Cart);
}

uint8_t readFromCartridge(Cartridge* Cart, uint16_t Address, CartRegion Region)
{
    if (!Cart)
        return 0xFF;

    switch (Cart->Mapper) {
    case MBC::MBC0:
        switch (Region) {
        case CartRegion::ROM0:
            return Cart->rom[0][Address];
        case CartRegion::ROM1:
            return Cart->rom[1][Address];
        case CartRegion::RAM:
            return (Cart->ramBanks > 0) ? Cart->ram[0][Address] : 0xFF;
        }
        break;

    case MBC::MBC1:
        switch (Region) {
        case CartRegion::ROM0:
            if (Cart->MBC1.memoryMode == 0) {
                return Cart->rom[0][Address];
            }
            else {
                int bank = (Cart->MBC1.currentRomBank2 << 5) & (Cart->romBanks - 1);
                bank = (Cart->romBanks > 32) ? bank : 0;
                return Cart->rom[bank][Address];
            }
        case CartRegion::ROM1:
        {
            int bank = ((Cart->MBC1.currentRomBank5 ? Cart->MBC1.currentRomBank5 : 1) |
                ((Cart->romBanks > 32) ? (Cart->MBC1.currentRomBank2 << 5) : 0)) &
                (Cart->romBanks - 1);
            return Cart->rom[bank][Address];
        }
        case CartRegion::RAM:
            if (Cart->ramBanks > 0 && Cart->MBC1.isRamEnabled) {
                int ram_bank = (Cart->MBC1.memoryMode == 0 || Cart->romBanks > 32)
                    ? 0
                    : (Cart->MBC1.currentRomBank2 & (Cart->ramBanks - 1));
                return Cart->ram[ram_bank][Address];
            }
            return 0xFF;
        }
        break;

    case MBC::MBC5:
        switch (Region) {
        case CartRegion::ROM0:
            return Cart->rom[0][Address];
        case CartRegion::ROM1:
        {
            int bank = Cart->MBC5.currentRomBank & (Cart->romBanks - 1);
            return Cart->rom[bank][Address];
        }
        case CartRegion::RAM:
            if (Cart->ramBanks > 0 && Cart->MBC5.isRamEnabled) {
                int ram_bank = Cart->MBC5.cur_ram_bank & (Cart->ramBanks - 1);
                return Cart->ram[ram_bank][Address];
            }
            return 0xFF;
        }
        break;

    default:
        return 0xFF;
    }

    return 0xFF;
}

void writeToCartridge(Cartridge* Cart, uint16_t Address, CartRegion Region, uint8_t Data)
{
    if (!Cart)
        return;

    switch (Cart->Mapper) {
    case MBC::MBC0:
        if (Region == CartRegion::RAM && Cart->ramBanks > 0) {
            Cart->ram[0][Address] = Data;
        }
        break;

    case MBC::MBC1:
        switch (Region) {
        case CartRegion::ROM0:
            if (Address < 0x2000) {
                if (Data == 0x0A || Data == 0x00) {
                    Cart->MBC1.isRamEnabled = Data;
                }
            }
            else {
                Cart->MBC1.currentRomBank5 = Data & 0b00011111;
            }
            break;
        case CartRegion::ROM1:
            if (Address < 0x2000) {
                Cart->MBC1.currentRomBank2 = Data & 0b00000011;
            }
            else {
                if (Data == 0x01 || Data == 0x00) {
                    Cart->MBC1.memoryMode = Data;
                }
            }
            break;
        case CartRegion::RAM:
            if (Cart->ramBanks > 0 && Cart->MBC1.isRamEnabled) {
                if (Cart->MBC1.memoryMode == 0 || Cart->romBanks > 32) {
                    Cart->ram[0][Address] = Data;
                }
                else {
                    int ram_bank = Cart->MBC1.currentRomBank2 & (Cart->ramBanks - 1);
                    Cart->ram[ram_bank][Address] = Data;
                }
            }
            break;
        }
        break;

    case MBC::MBC5:
        switch (Region) {
        case CartRegion::ROM0:
            if (Address < 0x2000) {
                if ((Data & 0x0F) == 0x0A || Data == 0x00) {
                    Cart->MBC5.isRamEnabled = Data;
                }
            }
            else {
                Cart->MBC5.currentRomBankLow = Data;
            }
            break;
        case CartRegion::ROM1:
            if (Address < 0x2000) {
                Cart->MBC5.currentRomBankHigh = Data & 0x01;
            }
            else {
                Cart->MBC5.cur_ram_bank = Data;
            }
            break;
        case CartRegion::RAM:
            if (Cart->ramBanks > 0 && Cart->MBC5.isRamEnabled) {
                int ram_bank = Cart->MBC5.cur_ram_bank & (Cart->ramBanks - 1);
                Cart->ram[ram_bank][Address] = Data;
            }
            break;
        }
        break;

    default:

        break;
    }
}
