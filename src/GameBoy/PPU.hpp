#pragma once

#include <SDL2/SDL.h>
#include <cstdint>

constexpr int16_t SCREEN_WIDTH = 160;
constexpr int16_t SCREEN_HEIGHT = 144;

constexpr int16_t CYCLES_PER_SCANLINE = 456;
constexpr int16_t TOTAL_SCANLINES = 154;
constexpr int16_t OAM_SCAN_CYCLES = 80;

constexpr int16_t TILE_SIZE_BYTES = 16;
constexpr int16_t TILEMAP_DIMENSION_BYTES = 32;

constexpr int8_t MAX_SPRITES_PER_SCANLINE = 10;
constexpr int8_t SPRITE_HEIGHT_NORMAL = 8;
constexpr int8_t SPRITE_HEIGHT_LARGE = 16;

const int32_t PALETTE_COLORS[4] = {
    0x00FFFFFF,
    0x00CCCCCC,
    0x00999999,
    0x00666666
};

enum LCDCFlags {
    LCDC_BG_DISPLAY = 0b00000001,
    LCDC_SPRITE_DISPLAY = 0b00000010,
    LCDC_SPRITE_SIZE = 0b00000100,
    LCDC_BG_MAP_SELECT = 0b00001000,
    LCDC_BG_TILE_SELECT = 0b00010000,
    LCDC_WINDOW_DISPLAY = 0b00100000,
    LCDC_WINDOW_TILE_SELECT = 0b01000000,
    LCDC_DISPLAY_ENABLE = 0b10000000
};

enum STATFlags {
    STAT_MODE = 0b0000011,
    STAT_LYCEQ = 0b0000100,
    STAT_INTERRUPT_HBLANK = 0b0001000,
    STAT_INTERRUPT_VBLANK = 0b0010000,
    STAT_INTERRUPT_OAM = 0b0100000,
    STAT_INTERRUPT_LYCEQ = 0b1000000
};

enum SpriteAttributes {
    SPRITE_PALETTE_NUMBER = 0b00010000,
    SPRITE_FLIP_HORIZONTAL = 0b00100000,
    SPRITE_FLIP_VERTICAL = 0b01000000,
    SPRITE_PRIORITY = 0b10000000
};

enum STATMode {
    STAT_MODE_HBLANK = 0,
    STAT_MODE_VBLANK = 1,
    STAT_MODE_OAM_SCAN = 2,
    STAT_MODE_PIXEL_RENDER = 3
};

struct GameBoy;

struct GameBoyPPU {

    GameBoy* GB;

    uint32_t* frameBuffer;
    int frameBufferPitch;

    int currentCycle;
    int currentScanline;
    int currentPixelX;
    bool isFrameComplete;
    bool isRenderingWindow;
    int windowScanline;

    uint8_t bgTileByte0;
    uint8_t bgTileByte1;
    uint8_t bgTileX;
    uint8_t bgTileY;
    uint8_t bgFineX;
    uint8_t bgFineY;

    uint8_t spriteTileByte0;
    uint8_t spriteTileByte1;
    uint8_t spritePalette;
    uint8_t spriteBGPriority;
    uint8_t activeSprites[10];
    uint8_t activeSpriteCount;
};

void PPUClock(GameBoyPPU* ppu);
