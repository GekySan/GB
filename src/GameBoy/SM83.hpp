#pragma once

#include <cstdint>

enum Flags {
    CARRY_FLAG      = 0b00010000,
    HALF_CARRY_FLAG = 0b00100000,
    SUBTRACT_FLAG   = 0b01000000,
    ZERO_FLAG       = 0b10000000
};

enum Condition {
    COND_NZ = 0,
    COND_Z  = 1,
    COND_NC = 2,
    COND_C  = 3
};

struct GameBoy;

struct SM83 {
    GameBoy* GB;

    union {
        uint16_t AF;
        struct {
            uint8_t F;
            uint8_t A;
        };
    };
    union {
        uint16_t BC;
        struct {
            uint8_t C;
            uint8_t B;
        };
    };
    union {
        uint16_t DE;
        struct {
            uint8_t E;
            uint8_t D;
        };
    };
    union {
        uint16_t HL;
        struct {
            uint8_t L;
            uint8_t H;
        };
    };

    uint16_t SP;
    uint16_t PC;

    bool IME;

    int currentCycles;

    bool ei;
    bool isHalted;
    bool isStopped;
    bool illegalOpcode;
};


void executeInstruction(SM83* CPU);
void executeOpcodeGroup0(SM83* CPU, uint8_t OPCode);
void executeOpcodeGroup1(SM83* CPU, uint8_t OPCode);
void executeOpcodeGroup2(SM83* CPU, uint8_t OPCode);
void executeOpcodeGroup3(SM83* CPU, uint8_t OPCode);
void executeGroup0Subgroup0(SM83* CPU, uint8_t OPCode);
void executeGroup0Subgroup1(SM83* CPU, uint8_t OPCode);
void executeGroup0Subgroup2(SM83* CPU, uint8_t OPCode);
void jumpRelative(SM83* CPU);
void jumpRelativeConditional(SM83* CPU, uint8_t OPCode);
void loadRegisterPairImmediate(SM83* CPU, uint8_t OPCode);
void addHLWithRegisterPair(SM83* CPU, uint8_t OPCode);
void loadMemoryWithA(SM83* CPU, uint8_t OPCode);
void loadAWithMemory(SM83* CPU, uint8_t OPCode);
void incrementRegisterPair(SM83* CPU, uint8_t OPCode);
void decrementRegisterPair(SM83* CPU, uint8_t OPCode);
void incrementRegister(SM83* CPU, uint8_t OPCode);
void decrementRegister(SM83* CPU, uint8_t OPCode);
void loadRegisterImmediate(SM83* CPU, uint8_t OPCode);
void executeSpecialOperation(SM83* CPU, uint8_t OPCode);
void rotateLeftCarryA(SM83* CPU);
void rotateRightCarryA(SM83* CPU);
void rotateLeftA(SM83* CPU);
void rotateRightA(SM83* CPU);
void decimalAdjustA(SM83* CPU);
void complementA(SM83* CPU);
void setCarryFlag(SM83* CPU);
void complementCarryFlag(SM83* CPU);
void returnConditional(SM83* CPU, uint8_t OPCode);
void popRegisterPair(SM83* CPU, uint8_t OPCode);
void jumpConditional(SM83* CPU, uint8_t OPCode);
void executeGroup3Specials0(SM83* CPU, uint8_t OPCode);
void executeGroup3Specials1(SM83* CPU, uint8_t OPCode);
void executeGroup3Specials2(SM83* CPU, uint8_t OPCode);
void executeGroup3Specials3(SM83* CPU, uint8_t OPCode);
void executePrefixCB(SM83* CPU);
void callConditional(SM83* CPU, uint8_t OPCode);
void pushRegisterPair(SM83* CPU, uint8_t OPCode);
void callImmediate(SM83* CPU);
void aluImmediate(SM83* CPU, uint8_t OPCode);
void restart(SM83* CPU, uint8_t OPCode);
void loadMemoryAddressWithSP(SM83* CPU);
void addSPImmediate(SM83* CPU);
void loadHLWithSPDisplacement(SM83* CPU);
void CPUClock(SM83* CPU);
