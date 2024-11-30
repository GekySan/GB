#include "APU.hpp"
#include "GB.hpp"
#include <cstdint>
#include <array>

static constexpr std::array<std::array<uint8_t, 8>, 4> _squareChannelDuty = { {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
} };

inline bool isDutyActive(const std::array<uint8_t, 8>& duty_currentCycle, uint8_t bit_index) noexcept {
    return duty_currentCycle[bit_index] != 0;
}

int8_t calculateChannel1Sample(const GameBoyAPU* apu) noexcept {
    uint8_t duty_type = static_cast<uint8_t>((apu->GB->io[NR11] & static_cast<uint8_t>(APUConstants::NRX1::DUTY_MASK)) >> 6);
    uint8_t current_currentCycle = static_cast<uint8_t>(apu->CH1.duty_index & 7);
    return isDutyActive(_squareChannelDuty[duty_type], current_currentCycle) ? static_cast<int8_t>(apu->CH1.volume) : 0;
}

int8_t calculateChannel2Sample(const GameBoyAPU* apu) noexcept {
    uint8_t duty_type = static_cast<uint8_t>((apu->GB->io[NR21] & static_cast<uint8_t>(APUConstants::NRX1::DUTY_MASK)) >> 6);
    uint8_t current_currentCycle = static_cast<uint8_t>(apu->CH2.duty_index & 7);
    return isDutyActive(_squareChannelDuty[duty_type], current_currentCycle) ? static_cast<int8_t>(apu->CH2.volume) : 0;
}

int8_t calculateChannel3Sample(const GameBoyAPU* apu) noexcept {
    uint8_t wvram_index = static_cast<uint8_t>((apu->CH3.audioSampleIndexex >> 1) & 0x0F);
    uint8_t sample = apu->GB->io[WAVERAM + wvram_index];

    sample = (apu->CH3.audioSampleIndexex & 1) ? static_cast<uint8_t>(sample & 0x0F) : static_cast<uint8_t>(sample >> 4);

    uint8_t out_level = static_cast<uint8_t>((apu->GB->io[NR32] & static_cast<uint8_t>(APUConstants::NR43::CLOCK_SHIFT_MASK)) >> 5);
    return out_level ? static_cast<int8_t>(sample >> (out_level - 1)) : 0;
}

int8_t calculateChannel4Sample(const GameBoyAPU* apu) noexcept {
    return (apu->CH4.lfsr & 1) ? apu->CH4.volume : 0;
}

inline void updateCounter(uint16_t& counter, uint16_t wavelen, uint8_t& index) noexcept {
    counter++;
    if (counter >= 2048) {
        counter = wavelen;
        index++;
    }
}

inline void updateEnvelope(uint8_t& env_counter, uint8_t env_pace, bool env_dir, uint8_t& volume) noexcept {
    if (env_pace > 0) {
        env_counter++;
        if (env_counter >= env_pace) {
            env_counter = 0;
            if (env_dir) {
                if (volume < 15) volume++;
            }
            else {
                if (volume > 0) volume--;
            }
        }
    }
}

void APUClock(GameBoyAPU* apu) {
    if (!(apu->GB->io[NR52] & static_cast<uint8_t>(APUConstants::NR52::APU_ENABLE_BIT))) {
        apu->GB->io[NR52] = 0;
        apu->apuDivider = 0;
        apu->audioSampleIndex = 0;
        return;
    }

    apu->GB->io[NR52] = static_cast<uint8_t>(APUConstants::NR52::APU_ENABLE_BIT) |
        (apu->CH1.enable ? 0x01 : 0) |
        (apu->CH2.enable ? 0x02 : 0) |
        (apu->CH3.enable ? 0x04 : 0) |
        (apu->CH4.enable ? 0x08 : 0);

    uint32_t div = apu->GB->div;

    if (div % 2 == 0) {
        apu->CH3.counter++;
        if (apu->CH3.counter >= 2048) {
            apu->CH3.counter = apu->CH3.wavelen;
            apu->CH3.audioSampleIndexex++;
        }
    }

    if (div % 4 == 0) {
        updateCounter(apu->CH1.counter, apu->CH1.wavelen, apu->CH1.duty_index);
        updateCounter(apu->CH2.counter, apu->CH2.wavelen, apu->CH2.duty_index);
        updateCounter(apu->CH3.counter, apu->CH3.wavelen, apu->CH3.audioSampleIndexex);
    }

    if (div % 8 == 0) {
        apu->CH4.counter++;
        int rate = 2 << ((apu->GB->io[NR43] & static_cast<uint8_t>(APUConstants::NR43::CLOCK_SHIFT_MASK)) >> 4);
        if (apu->GB->io[NR43] & static_cast<uint8_t>(APUConstants::NR43::DIVISOR_MASK)) {
            rate *= (apu->GB->io[NR43] & static_cast<uint8_t>(APUConstants::NR43::DIVISOR_MASK));
        }
        if (apu->CH4.counter >= rate) {
            apu->CH4.counter = 0;
            uint16_t bit = (~(apu->CH4.lfsr ^ (apu->CH4.lfsr >> 1))) & 1;
            apu->CH4.lfsr = (apu->CH4.lfsr & ~(1 << 15)) | (bit << 15);
            if (apu->GB->io[NR43] & static_cast<uint8_t>(APUConstants::NR43::WIDTH_MODE_BIT)) {
                apu->CH4.lfsr = (apu->CH4.lfsr & ~(1 << 7)) | (bit << 7);
            }
            apu->CH4.lfsr >>= 1;
        }
    }

    if (div % APUConstants::SAMPLE_RATE == 0) {
        int8_t ch1_sample = apu->CH1.enable ? calculateChannel1Sample(apu) : 0;
        int8_t ch2_sample = apu->CH2.enable ? calculateChannel2Sample(apu) : 0;
        int8_t ch3_sample = apu->CH3.enable ? calculateChannel3Sample(apu) : 0;
        int8_t ch4_sample = apu->CH4.enable ? calculateChannel4Sample(apu) : 0;

        uint8_t l_sample = 0, r_sample = 0;

        uint8_t nr51 = apu->GB->io[NR51];
        if (nr51 & 0x01) r_sample += ch1_sample;
        if (nr51 & 0x02) r_sample += ch2_sample;
        if (nr51 & 0x04) r_sample += ch3_sample;
        if (nr51 & 0x08) r_sample += ch4_sample;
        if (nr51 & 0x10) l_sample += ch1_sample;
        if (nr51 & 0x20) l_sample += ch2_sample;
        if (nr51 & 0x40) l_sample += ch3_sample;
        if (nr51 & 0x80) l_sample += ch4_sample;

        uint8_t nr50 = apu->GB->io[NR50];
        float left_volume = (static_cast<float>(l_sample) / 500.0f) *
            (static_cast<float>(((nr50 & static_cast<uint8_t>(APUConstants::NR50::LEFT_VOLUME_MASK)) >> 4) + 1));
        float right_volume = (static_cast<float>(r_sample) / 500.0f) *
            (static_cast<float>((nr50 & static_cast<uint8_t>(APUConstants::NR50::RIGHT_VOLUME_MASK)) + 1));

        apu->audioSampleBuffer[apu->audioSampleIndex++] = static_cast<float>(left_volume);
        apu->audioSampleBuffer[apu->audioSampleIndex++] = static_cast<float>(right_volume);

        if (apu->audioSampleIndex >= APUConstants::SAMPLE_BUF_LEN) {
            apu->isAudioBufferFull = true;
            apu->audioSampleIndex = 0;
        }
    }

    if (div % APUConstants::DIV_RATE == 0) {
        apu->apuDivider++;

        if (apu->apuDivider % 2 == 0) {

            auto updateLengthCounter = [&](uint8_t nrx4, auto& channel) {
                if (apu->GB->io[nrx4] & static_cast<uint8_t>(APUConstants::NRX4::LENGTH_ENABLE_BIT)) {
                    if (++channel.len_counter >= 64) {
                        channel.len_counter = 0;
                        channel.enable = false;
                    }
                }
                };

            updateLengthCounter(NR14, apu->CH1);
            updateLengthCounter(NR24, apu->CH2);
            updateLengthCounter(NR34, apu->CH3);
            updateLengthCounter(NR44, apu->CH4);
        }

        if (apu->apuDivider % 4 == 0) {

            apu->CH1.sweep_counter++;
            if (apu->CH1.sweep_pace && apu->CH1.sweep_counter >= apu->CH1.sweep_pace) {
                apu->CH1.sweep_counter = 0;
                uint8_t sweep_pace = (apu->GB->io[NR10] & static_cast<uint8_t>(APUConstants::NR10::SWEEP_PACE_MASK)) >> 4;
                uint8_t sweep_slope = apu->GB->io[NR10] & static_cast<uint8_t>(APUConstants::NR10::SWEEP_SLOPE_MASK);
                bool sweep_direction = (apu->GB->io[NR10] & static_cast<uint8_t>(APUConstants::NR10::SWEEP_DIR_BIT)) != 0;

                uint16_t delta_wave_length = apu->CH1.wavelen >> sweep_slope;
                int32_t new_wave_length = sweep_direction
                    ? static_cast<int32_t>(apu->CH1.wavelen) - delta_wave_length
                    : static_cast<int32_t>(apu->CH1.wavelen) + delta_wave_length;

                if ((sweep_direction && new_wave_length < 0) ||
                    (!sweep_direction && new_wave_length > 2048)) {
                    apu->CH1.enable = false;
                }
                else {
                    apu->CH1.wavelen = static_cast<uint16_t>(new_wave_length);
                }
            }
        }

        if (apu->apuDivider % 8 == 0) {

            updateEnvelope(apu->CH1.env_counter, apu->CH1.env_pace, apu->CH1.env_dir, apu->CH1.volume);
            updateEnvelope(apu->CH2.env_counter, apu->CH2.env_pace, apu->CH2.env_dir, apu->CH2.volume);
            updateEnvelope(apu->CH4.env_counter, apu->CH4.env_pace, apu->CH4.env_dir, apu->CH4.volume);
        }
    }
}
