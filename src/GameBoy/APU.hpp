#pragma once

#include <cstdint>
#include <array>
#include <memory>

namespace APUConstants {
    constexpr int BASE_FREQUENCY = 4'194'304;
    constexpr int DIV_RATE = 8192;
    constexpr int SAMPLE_FREQ = 22050;
    constexpr int SAMPLE_RATE = BASE_FREQUENCY / SAMPLE_FREQ;
    constexpr int SAMPLE_BUF_LEN = 1024;

    enum class NRX1 : uint8_t {
        LENGTH_MASK = 0b00111111,
        DUTY_MASK = 0b11000000
    };

    enum class NRX2 : uint8_t {
        ENVELOPE_PACE_MASK = 0b00000111,
        ENVELOPE_DIR_BIT = 0b00001000,
        VOLUME_MASK = 0b11110000
    };

    enum class NRX4 : uint8_t {
        WAVE_LENGTH_MASK = 0b00000111,
        LENGTH_ENABLE_BIT = 0b01000000,
        TRIGGER_BIT = 0b10000000
    };

    enum class NR10 : uint8_t {
        SWEEP_PACE_MASK = 0b01110000,
        SWEEP_SLOPE_MASK = 0b00000111,
        SWEEP_DIR_BIT = 0b00001000
    };

    enum class NR43 : uint8_t {
        CLOCK_SHIFT_MASK = 0b11110000,
        DIVISOR_MASK = 0b00000111,
        WIDTH_MODE_BIT = 0b00001000
    };

    enum class NR50 : uint8_t {
        LEFT_VOLUME_MASK = 0b01110000,
        RIGHT_VOLUME_MASK = 0b00000111
    };

    enum class NR51 : uint8_t {
        RIGHT_BITS = 0b00001111,
        LEFT_BITS = 0b11110000
    };

    enum class NR52 : uint8_t {
        APU_ENABLE_BIT = 0b10000000
    };
}

struct GameBoy;

struct Channel1 {
    bool enable = false;
    uint16_t counter = 0;
    uint16_t wavelen = 0;
    uint8_t duty_index = 0;
    uint8_t env_counter = 0;
    uint8_t env_pace = 0;
    bool env_dir = false;
    uint8_t volume = 0;
    uint8_t len_counter = 0;
    uint8_t sweep_pace = 0;
    uint8_t sweep_counter = 0;
};

struct Channel2 {
    bool enable = false;
    uint16_t counter = 0;
    uint16_t wavelen = 0;
    uint8_t duty_index = 0;
    uint8_t env_counter = 0;
    uint8_t env_pace = 0;
    bool env_dir = false;
    uint8_t volume = 0;
    uint8_t len_counter = 0;
};

struct Channel3 {
    bool enable = false;
    uint16_t counter = 0;
    uint16_t wavelen = 0;
    uint8_t audioSampleIndexex = 0;
    uint8_t len_counter = 0;
};

struct Channel4 {
    bool enable = false;
    int counter = 0;
    uint16_t lfsr = 0;
    uint8_t env_counter = 0;
    uint8_t env_pace = 0;
    bool env_dir = false;
    uint8_t volume = 0;
    uint8_t len_counter = 0;
};

struct GameBoyAPU {

    std::shared_ptr<GameBoy> GB;

    uint16_t apuDivider = 0;

    std::array<float, APUConstants::SAMPLE_BUF_LEN> audioSampleBuffer{};
    int audioSampleIndex = 0;
    bool isAudioBufferFull = false;

    Channel1 CH1;
    Channel2 CH2;
    Channel3 CH3;
    Channel4 CH4;

};

void APUClock(struct GameBoyAPU* apu);