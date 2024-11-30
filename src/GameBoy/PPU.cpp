#include <SDL2/SDL.h>

#include "GB.hpp"
#include "PPU.hpp"

uint8_t reverseByte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

bool isDisplayEnabled(const GameBoyPPU* ppu) {
    return ppu->GB->io[LCDC] & LCDC_DISPLAY_ENABLE;
}

bool isRenderingScanline(const GameBoyPPU* ppu) {
    return ppu->currentScanline < SCREEN_HEIGHT;
}

bool isVBlankCycle(const GameBoyPPU* ppu) {
    return (ppu->currentScanline == SCREEN_HEIGHT && ppu->currentCycle == 0);
}

bool canProcessSprites(const GameBoyPPU* ppu) {
    return !ppu->GB->dma_active && !(ppu->currentCycle & 1) && (ppu->activeSpriteCount < 10);
}

bool shouldRenderWindow(const GameBoyPPU* ppu) {
    return (ppu->GB->io[LCDC] & LCDC_WINDOW_DISPLAY) &&
        (ppu->currentPixelX == ppu->GB->io[WX] - 7) &&
        ppu->isRenderingWindow;
}

bool isSpriteOnTop(const GameBoyPPU* ppu, int bg_index, int obj_index) {
    return (bg_index == 0 || !(ppu->spriteBGPriority & 0x80)) && obj_index;
}

void resetPPU(GameBoyPPU* ppu) {
    ppu->currentCycle = 0;
    ppu->currentScanline = 0;
    ppu->GB->io[LY] = 0;
    ppu->GB->io[STAT] &= ~STAT_MODE;
}

void resetSpriteTiles(GameBoyPPU* ppu) {
    ppu->spriteTileByte0 = 0;
    ppu->spriteTileByte1 = 0;
    ppu->spritePalette = 0;
    ppu->spriteBGPriority = 0;
}

void loadBackgroundTile(GameBoyPPU* ppu) {
    uint16_t tilemap_offset;
    uint16_t tilemap_start;
    if ((ppu->GB->io[LCDC] & LCDC_WINDOW_DISPLAY) && ppu->isRenderingWindow &&
        ppu->currentPixelX >= (ppu->GB->io[WX] - 7)) {
        tilemap_offset =
            TILEMAP_DIMENSION_BYTES * ppu->bgTileY + ((ppu->bgTileX) & (TILEMAP_DIMENSION_BYTES - 1));
        tilemap_start =
            (ppu->GB->io[LCDC] & LCDC_WINDOW_TILE_SELECT) ? 0x1c00 : 0x1800;
    }
    else {
        tilemap_offset =
            TILEMAP_DIMENSION_BYTES * ppu->bgTileY +
            ((ppu->bgTileX + (ppu->GB->io[SCX] >> 3)) & (TILEMAP_DIMENSION_BYTES - 1));
        tilemap_start =
            (ppu->GB->io[LCDC] & LCDC_BG_MAP_SELECT) ? 0x1c00 : 0x1800;
    }
    uint8_t tile_index = ppu->GB->vram[0][tilemap_start + tilemap_offset];
    uint16_t tile_addr;
    if (ppu->GB->io[LCDC] & LCDC_BG_TILE_SELECT) {
        tile_addr = tile_index << 4;
    }
    else {
        tile_addr = 0x1000 + ((int8_t)tile_index << 4);
    }
    ppu->bgTileByte0 = ppu->GB->vram[0][tile_addr + 2 * ppu->bgFineY];
    ppu->bgTileByte1 = ppu->GB->vram[0][tile_addr + 2 * ppu->bgFineY + 1];
}

void loadSpriteTile(GameBoyPPU* ppu) {
    for (int i = 0; i < ppu->activeSpriteCount; i++) {
        if (ppu->currentPixelX != ppu->GB->oam[ppu->activeSprites[i] + 1] - 8) continue;
        int rel_y = ppu->currentScanline - ppu->GB->oam[ppu->activeSprites[i]] + 16;
        uint8_t tile_index = ppu->GB->oam[ppu->activeSprites[i] + 2];
        uint8_t obj_attr = ppu->GB->oam[ppu->activeSprites[i] + 3];
        if (ppu->GB->io[LCDC] & LCDC_SPRITE_SIZE) {
            if (obj_attr & SPRITE_FLIP_VERTICAL) rel_y = 15 - rel_y;
            tile_index &= ~1;
        }
        else {
            if (obj_attr & SPRITE_FLIP_VERTICAL) rel_y = 7 - rel_y;
        }
        uint8_t obj_b0 = ppu->GB->vram[0][(tile_index << 4) + 2 * rel_y];
        uint8_t obj_b1 = ppu->GB->vram[0][(tile_index << 4) + 2 * rel_y + 1];
        if (obj_attr & SPRITE_FLIP_HORIZONTAL) {
            obj_b0 = reverseByte(obj_b0);
            obj_b1 = reverseByte(obj_b1);
        }
        uint8_t mask = ~(ppu->spriteTileByte0 | ppu->spriteTileByte1);
        ppu->spriteTileByte0 |= obj_b0 & mask;
        ppu->spriteTileByte1 |= obj_b1 & mask;
        ppu->spritePalette &= ~mask;
        ppu->spriteBGPriority &= ~mask;
        if (obj_attr & SPRITE_PALETTE_NUMBER) ppu->spritePalette |= mask;
        if (obj_attr & SPRITE_PRIORITY) ppu->spriteBGPriority |= mask;
    }
}

void setupWindowRendering(GameBoyPPU* ppu) {
    ppu->bgTileX = 0;
    ppu->bgFineX = 0;
    ppu->bgTileY = (ppu->windowScanline >> 3) & (TILEMAP_DIMENSION_BYTES - 1);
    ppu->bgFineY = ppu->windowScanline & 0b111;
    ppu->windowScanline++;
    loadBackgroundTile(ppu);
}

void handleBackgroundRendering(GameBoyPPU* ppu, int& bg_index, int& color) {
    if (shouldRenderWindow(ppu)) {
        setupWindowRendering(ppu);
    }

    if (ppu->bgTileByte0 & 0x80) bg_index |= 0b01;
    if (ppu->bgTileByte1 & 0x80) bg_index |= 0b10;
    color = (ppu->GB->io[BGP] >> (2 * bg_index)) & 0b11;
}

int calculateSpriteIndex(const GameBoyPPU* ppu) {
    int obj_index = 0;
    if (ppu->spriteTileByte0 & 0x80) obj_index |= 0b01;
    if (ppu->spriteTileByte1 & 0x80) obj_index |= 0b10;
    return obj_index;
}

uint8_t getSpritePaletteColor(const GameBoyPPU* ppu, int obj_index) {
    if (ppu->spritePalette & 0x80) {
        return (ppu->GB->io[OBP1] >> (2 * obj_index)) & 0b11;
    }
    else {
        return (ppu->GB->io[OBP0] >> (2 * obj_index)) & 0b11;
    }
}

uint8_t getSpriteColor(GameBoyPPU* ppu, int bg_index) {
    loadSpriteTile(ppu);
    int obj_index = calculateSpriteIndex(ppu);

    if (isSpriteOnTop(ppu, bg_index, obj_index)) {
        return getSpritePaletteColor(ppu, obj_index);
    }

    return bg_index == 0 ? 0 : bg_index;
}

bool shouldRenderSprite(GameBoyPPU* ppu, int bg_index) {
    return !ppu->GB->dma_active &&
        (ppu->GB->io[LCDC] & LCDC_SPRITE_DISPLAY);
}

uint8_t calculatePixelColor(GameBoyPPU* ppu) {
    int bg_index = 0;
    int color = 0;

    if (ppu->GB->io[LCDC] & LCDC_BG_DISPLAY) {
        handleBackgroundRendering(ppu, bg_index, color);
    }

    if (shouldRenderSprite(ppu, bg_index)) {
        color = getSpriteColor(ppu, bg_index);
    }

    return color;
}

void renderPixel(GameBoyPPU* ppu, int color) {
    if (ppu->currentPixelX >= 0 && ppu->currentPixelX < SCREEN_WIDTH) {
        int pixelIndex = ppu->currentScanline * (ppu->frameBufferPitch / 4) + ppu->currentPixelX;
        ppu->frameBuffer[pixelIndex] = PALETTE_COLORS[color];
    }
}

void shiftSpriteTiles(GameBoyPPU* ppu) {
    ppu->spriteTileByte0 <<= 1;
    ppu->spriteTileByte1 <<= 1;
    ppu->spritePalette <<= 1;
    ppu->spriteBGPriority <<= 1;
}

void updateTileAndPixelCounters(GameBoyPPU* ppu) {
    ppu->bgTileByte0 <<= 1;
    ppu->bgTileByte1 <<= 1;
    ppu->bgFineX++;

    if (ppu->bgFineX == 8) {
        ppu->bgTileX++;
        ppu->bgFineX = 0;
    }

    ppu->currentPixelX++;
    shiftSpriteTiles(ppu);
}

void initializePixelRendering(GameBoyPPU* ppu) {
    ppu->GB->io[STAT] &= ~STAT_MODE;
    ppu->GB->io[STAT] |= STAT_MODE_PIXEL_RENDER;

    uint8_t curY = ppu->GB->io[SCY] + ppu->currentScanline;
    ppu->bgTileY = (curY >> 3) & (TILEMAP_DIMENSION_BYTES - 1);
    ppu->bgFineY = curY & 0b111;
    ppu->bgTileX = 0xff;
    ppu->bgFineX = ppu->GB->io[SCX] & 0b111;

    loadBackgroundTile(ppu);
    ppu->bgTileByte0 <<= ppu->bgFineX;
    ppu->bgTileByte1 <<= ppu->bgFineX;

    resetSpriteTiles(ppu);
}

void handlePixelRendering(GameBoyPPU* ppu) {
    if (ppu->currentPixelX == -8) {
        initializePixelRendering(ppu);
    }
    else if (ppu->bgFineX == 0) {
        loadBackgroundTile(ppu);
    }

    uint8_t color = calculatePixelColor(ppu);
    renderPixel(ppu, color);
    updateTileAndPixelCounters(ppu);
}

void processSprites(GameBoyPPU* ppu) {
    uint8_t obj_y = ppu->GB->oam[2 * ppu->currentCycle];
    int rel_y = ppu->currentScanline - obj_y + 16;
    uint8_t spriteHeight = (ppu->GB->io[LCDC] & LCDC_SPRITE_SIZE) ? 16 : 8;

    if (rel_y >= 0 && rel_y < spriteHeight) {
        ppu->activeSprites[ppu->activeSpriteCount++] = 2 * ppu->currentCycle;
    }
}

void enterOAMScanMode(GameBoyPPU* ppu) {
    ppu->GB->io[STAT] &= ~STAT_MODE;
    ppu->GB->io[STAT] |= STAT_MODE_OAM_SCAN;

    if (ppu->currentScanline == 0) {
        ppu->isRenderingWindow = false;
    }

    if (ppu->currentScanline == ppu->GB->io[WY]) {
        ppu->windowScanline = 0;
        ppu->isRenderingWindow = true;
    }

    ppu->currentPixelX = -8;
    ppu->activeSpriteCount = 0;
}

void handleOAMScan(GameBoyPPU* ppu) {
    if (ppu->currentCycle == 0) {
        enterOAMScanMode(ppu);
    }

    if (canProcessSprites(ppu)) {
        processSprites(ppu);
    }
}

void finalizeScanlineRendering(GameBoyPPU* ppu) {
    ppu->GB->io[STAT] &= ~STAT_MODE;
}

void handleVBlank(GameBoyPPU* ppu) {
    ppu->GB->io[IF] |= INTERRUPT_VBLANK;
    ppu->GB->io[STAT] &= ~STAT_MODE;
    ppu->GB->io[STAT] |= STAT_MODE_VBLANK;
}

void incrementCycleAndScanline(GameBoyPPU* ppu) {
    ppu->currentCycle++;
    if (ppu->currentCycle == CYCLES_PER_SCANLINE) {
        ppu->currentCycle = 0;
        ppu->currentScanline++;

        if (ppu->currentScanline == TOTAL_SCANLINES) {
            ppu->currentScanline = 0;
            ppu->isFrameComplete = true;
        }

        ppu->GB->io[LY] = ppu->currentScanline;
    }
}

void handleRenderingCycle(GameBoyPPU* ppu) {
    if (ppu->currentCycle < OAM_SCAN_CYCLES) {
        handleOAMScan(ppu);
    }
    else if (ppu->currentPixelX < SCREEN_WIDTH) {
        handlePixelRendering(ppu);
    }
    else if (ppu->currentPixelX == SCREEN_WIDTH) {
        finalizeScanlineRendering(ppu);
    }
}

void PPUClock(GameBoyPPU* ppu) {
    if (!isDisplayEnabled(ppu)) {
        resetPPU(ppu);
        return;
    }

    if (isRenderingScanline(ppu)) {
        handleRenderingCycle(ppu);
    }
    else if (isVBlankCycle(ppu)) {
        handleVBlank(ppu);
    }

    incrementCycleAndScanline(ppu);
}
