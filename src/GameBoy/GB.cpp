#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <SDL2/SDL.h>
#include <string>

#include "Cartridge.hpp"
#include "GB.hpp"

uint8_t readMemoryByte(GameBoy* bus, uint16_t addr) {
    bus->CPU.currentCycles += 4;

    if (bus->dma_active && addr < 0xFF00) {
        return 0xFF;
    }

    if (addr >= 0x0000 && addr <= 0x3FFF) {
        return readFromCartridge(bus->cart, addr, CartRegion::ROM0);
    }
    else if (addr >= 0x4000 && addr <= 0x7FFF) {
        return readFromCartridge(bus->cart, addr & 0x3FFF, CartRegion::ROM1);
    }
    else if (addr >= 0x8000 && addr <= 0x9FFF) {
        if (!(bus->io[LCDC] & LCDC_DISPLAY_ENABLE) ||
            (bus->io[STAT] & STAT_MODE) != 3) {
            return bus->vram[0][addr & 0x1FFF];
        }
        return 0xFF;
    }
    else if (addr >= 0xA000 && addr <= 0xBFFF) {
        return readFromCartridge(bus->cart, addr & 0x1FFF, CartRegion::RAM);
    }
    else if (addr >= 0xC000 && addr <= 0xCFFF) {
        return bus->wram[0][addr & 0x0FFF];
    }
    else if (addr >= 0xD000 && addr <= 0xDFFF) {
        return bus->wram[1][addr & 0x0FFF];
    }
    else if (addr >= 0xE000 && addr <= 0xFDFF) {
        uint16_t mirrored_addr = addr - 0x2000;
        if (mirrored_addr < 0xC000 || mirrored_addr >= 0xE000) {
            return 0xFF;
        }
        uint8_t bank = mirrored_addr < 0xD000 ? 0 : 1;
        return bus->wram[bank][mirrored_addr & 0x0FFF];
    }
    else if (addr >= 0xFE00 && addr <= 0xFE9F) {
        if (!(bus->io[LCDC] & LCDC_DISPLAY_ENABLE) || (bus->io[STAT] & STAT_MODE) < 2) {
            return bus->oam[addr - 0xFE00];
        }
        return 0xFF;
    }
    else if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        return 0xFF;
    }
    else if (addr >= 0xFF00 && addr <= 0xFF7F) {
        if (addr == 0xFF04) {
            return (bus->div >> 8) & 0xFF;
        }
        else if (addr >= 0xFF30 && addr <= 0xFF3F) {
            return bus->apu.CH3.enable ? 0xFF : bus->io[addr - 0xFF30];
        }
        return bus->io[addr & 0x00FF];
    }
    else if (addr >= 0xFF80 && addr <= 0xFFFE) {
        return bus->hram[addr - 0xFF80];
    }
    else if (addr == 0xFFFF) {
        return bus->IE;
    }

    return 0xFF;
}

void writeMemoryByte(GameBoy* bus, uint16_t addr, uint8_t data) {
    bus->CPU.currentCycles += 4;

    if (bus->dma_active && addr < 0xff00) return;
    if (addr < 0x4000) {
        writeToCartridge(bus->cart, addr, CartRegion::ROM0, data);
        return;
    }
    if (addr < 0x8000) {
        writeToCartridge(bus->cart, addr & 0x3fff, CartRegion::ROM1, data);
        return;
    }
    if (addr < 0xa000) {
        if (!(bus->io[LCDC] & LCDC_DISPLAY_ENABLE) ||
            (bus->io[STAT] & STAT_MODE) != 3) {
            bus->vram[0][addr & 0x1fff] = data;
        }
        return;
    }
    if (addr < 0xc000) {
        writeToCartridge(bus->cart, addr & 0x1fff, CartRegion::RAM, data);
        return;
    }
    if (addr < 0xd000) {
        bus->wram[0][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xe000) {
        bus->wram[1][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xfe00) {
        bus->wram[0][addr & 0x0fff] = data;
        return;
    }
    if (addr < 0xfea0) {
        if (!(bus->io[LCDC] & LCDC_DISPLAY_ENABLE) || (bus->io[STAT] & STAT_MODE) < 2) {
            bus->oam[addr - 0xfe00] = data;
        }
        return;
    }
    if (addr < 0xff00) {
        return;
    }
    if (addr < 0xff80) {
        if (!bus->apu.CH3.enable && (addr & 0x00f0) == WAVERAM) {
            (bus->io + WAVERAM)[addr & 0x000f] = data;
            return;
        }
        switch (addr & 0x00FF) {
        case JOYP:
            bus->io[JOYP] = (bus->io[JOYP] & 0b11001111) | (data & 0b00110000);
            break;
        case DIV:
            bus->div = 0x0000;
            break;
        case TIMA:
            bus->io[TIMA] = data;
            break;
        case TMA:
            bus->io[TMA] = data;
            break;
        case TAC:
            bus->io[TAC] = data & 0b0111;
            break;
        case IF:
            bus->io[IF] = data & 0b00011111;
            break;
        case NR10:
            if ((data & static_cast<uint8_t>(APUConstants::NR10::SWEEP_PACE_MASK)) == 0) {
                bus->apu.CH1.sweep_pace = 0;
            }
            bus->io[NR10] = data;
            break;
        case NR11:
            bus->apu.CH1.len_counter = data & static_cast<uint8_t>(APUConstants::NRX1::LENGTH_MASK);
            bus->io[NR11] = data & static_cast<uint8_t>(APUConstants::NRX1::DUTY_MASK);
            break;
        case NR12:
            if (!(data & 0b11111000)) bus->apu.CH1.enable = false;
            bus->io[NR12] = data;
            break;
        case NR13:
            bus->apu.CH1.wavelen = (bus->apu.CH1.wavelen & 0xFF00) | data;
            break;
        case NR14:
            bus->apu.CH1.wavelen = (bus->apu.CH1.wavelen & 0x00FF) |
                ((data & static_cast<uint8_t>(APUConstants::NRX4::WAVE_LENGTH_MASK)) << 8);
            if ((bus->io[NR12] & 0b11111000) &&
                (data & static_cast<uint8_t>(APUConstants::NRX4::TRIGGER_BIT))) {
                bus->apu.CH1.enable = true;
                bus->apu.CH1.counter = bus->apu.CH1.wavelen;
                bus->apu.CH1.duty_index = 0;
                bus->apu.CH1.env_counter = 0;
                bus->apu.CH1.env_pace = bus->io[NR12] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_PACE_MASK);
                bus->apu.CH1.env_dir = (bus->io[NR12] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_DIR_BIT)) != 0;
                bus->apu.CH1.volume = (bus->io[NR12] & static_cast<uint8_t>(APUConstants::NRX2::VOLUME_MASK)) >> 4;
                bus->apu.CH1.sweep_pace = (bus->io[NR10] & static_cast<uint8_t>(APUConstants::NR10::SWEEP_PACE_MASK)) >> 4;
                bus->apu.CH1.sweep_counter = 0;
            }
            bus->io[NR14] = data & static_cast<uint8_t>(APUConstants::NRX4::LENGTH_ENABLE_BIT);
            break;
        case NR21:
            bus->apu.CH2.len_counter = data & static_cast<uint8_t>(APUConstants::NRX1::LENGTH_MASK);
            bus->io[NR21] = data & static_cast<uint8_t>(APUConstants::NRX1::DUTY_MASK);
            break;
        case NR22:
            if (!(data & 0b11111000)) bus->apu.CH2.enable = false;
            bus->io[NR22] = data;
            break;
        case NR23:
            bus->apu.CH2.wavelen = (bus->apu.CH2.wavelen & 0xFF00) | data;
            break;
        case NR24:
            bus->apu.CH2.wavelen = (bus->apu.CH2.wavelen & 0x00FF) |
                ((data & static_cast<uint8_t>(APUConstants::NRX4::WAVE_LENGTH_MASK)) << 8);
            if ((bus->io[NR22] & 0b11111000) &&
                (data & static_cast<uint8_t>(APUConstants::NRX4::TRIGGER_BIT))) {
                bus->apu.CH2.enable = true;
                bus->apu.CH2.counter = bus->apu.CH2.wavelen;
                bus->apu.CH2.duty_index = 0;
                bus->apu.CH2.env_counter = 0;
                bus->apu.CH2.env_pace = bus->io[NR22] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_PACE_MASK);
                bus->apu.CH2.env_dir = (bus->io[NR22] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_DIR_BIT)) != 0;
                bus->apu.CH2.volume = (bus->io[NR22] & static_cast<uint8_t>(APUConstants::NRX2::VOLUME_MASK)) >> 4;
            }
            bus->io[NR24] = data & static_cast<uint8_t>(APUConstants::NRX4::LENGTH_ENABLE_BIT);
            break;
        case NR30:
            if (!(data & 0b10000000)) bus->apu.CH3.enable = false;
            bus->io[NR30] = data & 0b10000000;
            break;
        case NR31:
            bus->apu.CH3.len_counter = data;
            break;
        case NR32:
            bus->io[NR32] = data & 0b01100000;
            break;
        case NR33:
            bus->apu.CH3.wavelen = (bus->apu.CH3.wavelen & 0xFF00) | data;
            break;
        case NR34:
            bus->apu.CH3.wavelen = (bus->apu.CH3.wavelen & 0x00FF) |
                ((data & static_cast<uint8_t>(APUConstants::NRX4::WAVE_LENGTH_MASK)) << 8);
            if ((bus->io[NR30] & 0b10000000) &&
                (data & static_cast<uint8_t>(APUConstants::NRX4::TRIGGER_BIT))) {
                bus->apu.CH3.enable = true;
                bus->apu.CH3.counter = bus->apu.CH3.wavelen;
                bus->apu.CH3.audioSampleIndexex = 0;
            }
            bus->io[NR34] = data & static_cast<uint8_t>(APUConstants::NRX4::LENGTH_ENABLE_BIT);
            break;
        case NR41:
            bus->apu.CH4.len_counter = data & static_cast<uint8_t>(APUConstants::NRX1::LENGTH_MASK);
            break;
        case NR42:
            if (!(data & 0b11111000)) bus->apu.CH4.enable = false;
            bus->io[NR42] = data;
            break;
        case NR43:
            bus->io[NR43] = data;
            break;
        case NR44:
            if ((bus->io[NR42] & 0b11111000) &&
                (data & static_cast<uint8_t>(APUConstants::NRX4::TRIGGER_BIT))) {
                bus->apu.CH4.enable = true;
                bus->apu.CH4.counter = 0;
                bus->apu.CH4.lfsr = 0;
                bus->apu.CH4.env_counter = 0;
                bus->apu.CH4.env_pace = bus->io[NR42] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_PACE_MASK);
                bus->apu.CH4.env_dir = (bus->io[NR42] & static_cast<uint8_t>(APUConstants::NRX2::ENVELOPE_DIR_BIT)) != 0;
                bus->apu.CH4.volume = (bus->io[NR42] & static_cast<uint8_t>(APUConstants::NRX2::VOLUME_MASK)) >> 4;
            }
            bus->io[NR44] = data & static_cast<uint8_t>(APUConstants::NRX4::LENGTH_ENABLE_BIT);
            break;
        case NR50:
            bus->io[NR50] = data;
            break;
        case NR51:
            bus->io[NR51] = data;
            break;
        case NR52:
            bus->io[NR52] = data & static_cast<uint8_t>(APUConstants::NR52::APU_ENABLE_BIT);
            break;
        case LCDC:
            bus->io[LCDC] = data;
            break;
        case STAT:
            bus->io[STAT] = (bus->io[STAT] & 0b000111) | (data & 0b01111000);
            break;
        case SCY:
            bus->io[SCY] = data;
            break;
        case SCX:
            bus->io[SCX] = data;
            break;
        case LYC:
            bus->io[LYC] = data;
            break;
        case DMA:
            bus->io[DMA] = data;
            bus->dma_active = true;
            bus->dma_index = 0;
            break;
        case BGP:
            bus->io[BGP] = data;
            break;
        case OBP0:
            bus->io[OBP0] = data;
            break;
        case OBP1:
            bus->io[OBP1] = data;
            break;
        case WY:
            bus->io[WY] = data;
            break;
        case WX:
            bus->io[WX] = data;
            break;
        }

    }
    if (addr < 0xffff) {
        bus->hram[addr - 0xff80] = data;
        return;
    }
    if (addr == 0xffff) bus->IE = data & 0b00011111;
}

uint16_t readMemoryWord(GameBoy* bus, uint16_t addr) {
    return readMemoryByte(bus, addr) | ((uint16_t)readMemoryByte(bus, addr + 1) << 8);
}

void writeMemoryWord(GameBoy* bus, uint16_t addr, uint16_t data) {
    writeMemoryByte(bus, addr, static_cast<uint8_t>(data));
    writeMemoryByte(bus, addr + 1, static_cast<uint8_t>(data >> 8));
}

void emulateCycle(struct GameBoy* gb) {
    checkStatusInterrupt(gb);
    updateTimers(gb);
    updateJoypadState(gb);
    if (gb->dma_active) executeDMA(gb);
    PPUClock(&gb->ppu);
    APUClock(&gb->apu);
    CPUClock(&gb->CPU);
}

void checkStatusInterrupt(struct GameBoy* gb) {
    if (gb->io[LYC] == gb->io[LY]) {
        gb->io[STAT] |= STAT_LYCEQ;
    }
    else {
        gb->io[STAT] &= ~STAT_LYCEQ;
    }
    bool new_stat_int =
        ((gb->io[STAT] & STAT_LYCEQ) && (gb->io[STAT] & STAT_INTERRUPT_LYCEQ)) ||
        ((gb->io[STAT] & STAT_MODE) == 0 && (gb->io[STAT] & STAT_INTERRUPT_HBLANK)) ||
        ((gb->io[STAT] & STAT_MODE) == 1 && (gb->io[STAT] & STAT_INTERRUPT_VBLANK)) ||
        ((gb->io[STAT] & STAT_MODE) == 2 && (gb->io[STAT] & STAT_INTERRUPT_OAM));
    if (new_stat_int && !gb->prev_stat_int) gb->io[IF] |= INTERRUPT_STATUS;
    gb->prev_stat_int = new_stat_int;
}

void updateTimers(GameBoy* gb_system) {
    gb_system->div++;
    if (gb_system->timer_overflow) {
        gb_system->io[IF] |= INTERRUPT_TIMER;
        gb_system->io[TIMA] = gb_system->io[TMA];
        gb_system->timer_overflow = false;
    }
    static const int freq[] = { 1024, 16, 64, 256 };
    bool new_timer_inc =
        (gb_system->io[TAC] & 0b100) && (gb_system->div & (freq[gb_system->io[TAC] & 0b011]) / 2);
    if (!new_timer_inc && gb_system->prev_timer_inc) {
        gb_system->io[TIMA]++;
        if (gb_system->io[TIMA] == 0) {
            gb_system->timer_overflow = true;

        }
    }
    gb_system->prev_timer_inc = new_timer_inc;
}

void updateJoypadState(struct GameBoy* gb) {
    uint8_t buttons = 0b11110000;
    if (!(gb->io[JOYP] & JOYPAD_DIRECTIONAL)) {
        buttons |= gb->jp_dir;
    }
    if (!(gb->io[JOYP] & JOYPAD_ACTION)) {
        buttons |= gb->jp_action;
    }
    buttons = ~buttons;
    if (buttons < (gb->io[JOYP] & 0b1111)) {
        gb->io[IF] |= INTERRUPT_JOYPAD;
    }
    gb->io[JOYP] = (gb->io[JOYP] & 0b11110000) | buttons;
}

void executeDMA(struct GameBoy* gb) {
    if (gb->dma_currentCycles == 0) {
        if (gb->dma_index == OAM_SIZE) {
            gb->dma_active = false;
            return;
        }
        gb->dma_currentCycles += 4;
        uint16_t addr = gb->io[DMA] << 8 | gb->dma_index;
        uint8_t data;
        if (addr < 0x4000) {
            data = readFromCartridge(gb->cart, addr & 0x3fff, CartRegion::ROM0);
        }
        else if (addr < 0x8000) {
            data = readFromCartridge(gb->cart, addr & 0x3fff, CartRegion::ROM1);
        }
        else if (addr < 0xa000) {
            data = gb->vram[0][addr & 0x1fff];
        }
        else if (addr < 0xc000) {
            data = readFromCartridge(gb->cart, addr & 0x1fff, CartRegion::RAM);
        }
        else if (addr < 0xd000) {
            data = gb->wram[0][addr & 0x0fff];
        }
        else if (addr < 0xe000) {
            data = gb->wram[1][addr & 0x0fff];
        }
        else {
            data = 0xff;
        }
        gb->oam[gb->dma_index] = data;
        gb->dma_index++;
    }
    gb->dma_currentCycles--;
}

void resetGameBoy(struct GameBoy* gb, struct Cartridge* cart) {
    memset(gb, 0x00, sizeof * gb);
    gb->CPU.GB = gb;
    gb->ppu.GB = gb;

    gb->apu.GB = std::shared_ptr<GameBoy>(gb);

    gb->cart = cart;
    gb->CPU.A = 0x01;
    gb->CPU.SP = 0xfffe;
    gb->CPU.PC = 0x0100;
    gb->io[LCDC] |= LCDC_DISPLAY_ENABLE;
}

std::string generateScreenshotFilename() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "ScreenShot_%d-%m-%Y_%H-%M-%S.bmp", &tm);
    return std::string(buffer);
}

bool captureScreenshot(const GameBoy* gb_instance, const std::string& filename) {
    if (!gb_instance->renderer) {
        std::cerr << "Renderer non initialisé!" << std::endl;
        return false;
    }

    int width, height;
    SDL_GetRendererOutputSize(gb_instance->renderer, &width, &height);

    SDL_Surface* screen_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!screen_surface) {
        std::cerr << "Erreur lors de la création de la surface: " << SDL_GetError() << std::endl;
        return false;
    }

    if (SDL_RenderReadPixels(gb_instance->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screen_surface->pixels, screen_surface->pitch) != 0) {
        std::cerr << "Erreur lors de la lecture des pixels: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(screen_surface);
        return false;
    }

    if (SDL_SaveBMP(screen_surface, filename.c_str()) != 0) {
        std::cerr << "Erreur lors de la sauvegarde de l'image: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(screen_surface);
        return false;
    }

    std::cout << "Capture d'écran effectuée -> " << filename << std::endl;
    SDL_FreeSurface(screen_surface);
    return true;
}

void handleGameBoyEvent(struct GameBoy* gb, SDL_Event* e) {

    if (e->type != SDL_CONTROLLERBUTTONDOWN && e->type != SDL_CONTROLLERBUTTONUP) {
        return;
    }

    bool pressed = (e->type == SDL_CONTROLLERBUTTONDOWN);

    auto setFlag = [&](uint8_t& field, uint8_t flag) {
        if (pressed) {
            field |= flag;
        }
        else {
            field &= ~flag;
        }
        };

    SDL_GameControllerButton button = static_cast<SDL_GameControllerButton>(e->cbutton.button);

    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        setFlag(gb->jp_dir, JOYPAD_UP_SELECT);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        setFlag(gb->jp_dir, JOYPAD_DOWN_START);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        setFlag(gb->jp_dir, JOYPAD_LEFT_B);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        setFlag(gb->jp_dir, JOYPAD_RIGHT_A);
        break;
    case SDL_CONTROLLER_BUTTON_A:
        setFlag(gb->jp_action, JOYPAD_RIGHT_A);
        break;
    case SDL_CONTROLLER_BUTTON_B:
        setFlag(gb->jp_action, JOYPAD_LEFT_B);
        break;
    case SDL_CONTROLLER_BUTTON_BACK:
        setFlag(gb->jp_action, JOYPAD_UP_SELECT);
        break;
    case SDL_CONTROLLER_BUTTON_START:
        setFlag(gb->jp_action, JOYPAD_DOWN_START);
        break;
    case SDL_CONTROLLER_BUTTON_MISC1:
        if (pressed) {
            std::string filename = generateScreenshotFilename();
            if (!captureScreenshot(gb, filename)) {
                std::cerr << "Échec de la capture d'écran." << std::endl;
            }
        }
        break;

    default:
        break;
    }
}