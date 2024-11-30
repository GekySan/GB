#pragma once

#include "APU.hpp"
#include "Cartridge.hpp"
#include "LocaleInitializer.hpp"
#include "SM83.hpp"
#include "PPU.hpp"

#include <cstdint>
#include <memory>

constexpr int16_t VRAM_BANK_SIZE = 8 * 1024;
constexpr int16_t WRAM_BANK_SIZE = 4 * 1024;

constexpr uint8_t OAM_SIZE = 0xA0;
constexpr uint8_t IO_SIZE = 0x80;
constexpr uint8_t HRAM_SIZE = 0x7F;

enum InterruptFlags {
    INTERRUPT_VBLANK = 0b00001,
    INTERRUPT_STATUS = 0b00010,
    INTERRUPT_TIMER = 0b00100,
    INTERRUPT_SERIAL = 0b01000,
    INTERRUPT_JOYPAD = 0b10000
};

enum JoypadFlags {
    JOYPAD_RIGHT_A = 0b000001,
    JOYPAD_LEFT_B = 0b000010,
    JOYPAD_UP_SELECT = 0b000100,
    JOYPAD_DOWN_START = 0b001000,
    JOYPAD_DIRECTIONAL = 0b010000,
    JOYPAD_ACTION = 0b100000
};

enum {
    JOYP = 0x00,
    SB = 0x01,
    SC = 0x02,
    DIV = 0x04,
    TIMA = 0x05,
    TMA = 0x06,
    TAC = 0x07,
    IF = 0x0F,

    NR10 = 0x10,
    NR11 = 0x11,
    NR12 = 0x12,
    NR13 = 0x13,
    NR14 = 0x14,
    NR21 = 0x16,
    NR22 = 0x17,
    NR23 = 0x18,
    NR24 = 0x19,
    NR30 = 0x1A,
    NR31 = 0x1B,
    NR32 = 0x1C,
    NR33 = 0x1D,
    NR34 = 0x1E,
    NR41 = 0x20,
    NR42 = 0x21,
    NR43 = 0x22,
    NR44 = 0x23,
    NR50 = 0x24,
    NR51 = 0x25,
    NR52 = 0x26,
    WAVERAM = 0x30,

    LCDC = 0x40,
    STAT = 0x41,
    SCY = 0x42,
    SCX = 0x43,
    LY = 0x44,
    LYC = 0x45,
    DMA = 0x46,
    BGP = 0x47,
    OBP0 = 0x48,
    OBP1 = 0x49,
    WY = 0x4A,
    WX = 0x4B,
};

struct GameBoy {
    SDL_Renderer* renderer;

    SM83 CPU;
    GameBoyPPU ppu;
    GameBoyAPU apu;

    Cartridge* cart;

    uint8_t vram[1][VRAM_BANK_SIZE];
    uint8_t wram[2][WRAM_BANK_SIZE];

    uint8_t oam[OAM_SIZE];

    uint8_t io[IO_SIZE];

    uint8_t hram[HRAM_SIZE];

    uint8_t IE;

    uint16_t div;

    bool prev_timer_inc;
    bool timer_overflow;

    bool prev_stat_int;

    uint8_t jp_dir;
    uint8_t jp_action;

    bool dma_active;
    uint8_t dma_index;
    int dma_currentCycles;

};

uint8_t readMemoryByte(GameBoy* bus, uint16_t addr);
uint16_t readMemoryWord(GameBoy* bus, uint16_t addr);
void writeMemoryByte(GameBoy* bus, uint16_t addr, uint8_t data);
void writeMemoryWord(GameBoy* bus, uint16_t addr, uint16_t data);

void handleGameBoyEvent(struct GameBoy* gb, SDL_Event* e);

void checkStatusInterrupt(struct GameBoy* gb);
void updateTimers(GameBoy* gb);
void updateJoypadState(struct GameBoy* gb);
void executeDMA(struct GameBoy* gb);

void emulateCycle(struct GameBoy* gb);

void resetGameBoy(struct GameBoy* gb, struct Cartridge* cart);