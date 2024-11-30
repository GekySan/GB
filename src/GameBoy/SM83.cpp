#include "SM83.hpp"

#include "GB.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

void setFlag(SM83* CPU, uint8_t flag, bool value) {
    if (value) {
        CPU->F |= flag;
    }
    else {
        CPU->F &= ~flag;
    }
}

void resolveFlags(SM83* CPU, uint8_t flags, uint8_t pre, uint8_t post, bool carry) {
    setFlag(CPU, SUBTRACT_FLAG, flags & SUBTRACT_FLAG);
    if (flags & ZERO_FLAG) {
        setFlag(CPU, ZERO_FLAG, post == 0);
    }
    if (flags & HALF_CARRY_FLAG) {

        setFlag(CPU, HALF_CARRY_FLAG,
            (flags & SUBTRACT_FLAG)
            ? ((pre & 0x0F) < (post & 0x0F) || ((pre & 0x0F) == (post & 0x0F) && carry))
            : ((pre & 0x0F) > (post & 0x0F) || ((pre & 0x0F) == (post & 0x0F) && carry)));
    }
    if (flags & CARRY_FLAG) {

        setFlag(CPU, CARRY_FLAG,
            (flags & SUBTRACT_FLAG)
            ? (pre < post || (pre == post && carry))
            : (pre > post || (pre == post && carry)));
    }
}

bool evalCondition(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00011000) >> 3) {
    case COND_NZ: return !(CPU->F & ZERO_FLAG);
    case COND_Z:  return  (CPU->F & ZERO_FLAG);
    case COND_NC: return !(CPU->F & CARRY_FLAG);
    case COND_C:  return  (CPU->F & CARRY_FLAG);
    }
    return false;
}

uint16_t* getRegisterPointer16(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00110000) >> 4) {
    case 0: return &CPU->BC;
    case 1: return &CPU->DE;
    case 2: return &CPU->HL;
    case 3: return &CPU->SP;
    }
    return nullptr;
}

uint16_t* getStackPointer16(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00110000) >> 4) {
    case 0: return &CPU->BC;
    case 1: return &CPU->DE;
    case 2: return &CPU->HL;
    case 3: return &CPU->AF;
    }
    return nullptr;
}

uint16_t getAddressWithIncrementOrDecrement(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00110000) >> 4) {
    case 0: return CPU->BC;
    case 1: return CPU->DE;
    case 2: return CPU->HL++;
    case 3: return CPU->HL--;
    }
    return 0;
}

uint8_t getSourceRegisterValue8(SM83* CPU, uint8_t OPCode) {
    switch (OPCode & 0b00000111) {
    case 0: return CPU->B;
    case 1: return CPU->C;
    case 2: return CPU->D;
    case 3: return CPU->E;
    case 4: return CPU->H;
    case 5: return CPU->L;
    case 6: return readMemoryByte(CPU->GB, CPU->HL);
    case 7: return CPU->A;
    }
    return 0;
}

uint8_t* getDestinationRegisterPointer8(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00111000) >> 3) {
    case 0: return &CPU->B;
    case 1: return &CPU->C;
    case 2: return &CPU->D;
    case 3: return &CPU->E;
    case 4: return &CPU->H;
    case 5: return &CPU->L;
    case 6: return nullptr;
    case 7: return &CPU->A;
    }
    return nullptr;
}

void executeALUOperation(SM83* CPU, uint8_t OPCode, uint8_t OPCode2) {
    uint8_t pre = CPU->A;
    switch ((OPCode & 0b00111000) >> 3) {
    case 0: CPU->A += OPCode2; resolveFlags(CPU, ZERO_FLAG | HALF_CARRY_FLAG | CARRY_FLAG, pre, CPU->A, 0); break;
    case 1: CPU->A += OPCode2 + (CPU->F & CARRY_FLAG ? 1 : 0); resolveFlags(CPU, ZERO_FLAG | HALF_CARRY_FLAG | CARRY_FLAG, pre, CPU->A, CPU->F & CARRY_FLAG); break;
    case 2: CPU->A -= OPCode2; resolveFlags(CPU, ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG | CARRY_FLAG, pre, CPU->A, 0); break;
    case 3: CPU->A -= OPCode2 + (CPU->F & CARRY_FLAG ? 1 : 0); resolveFlags(CPU, ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG | CARRY_FLAG, pre, CPU->A, CPU->F & CARRY_FLAG); break;
    case 4: CPU->A &= OPCode2; setFlag(CPU, ZERO_FLAG, CPU->A == 0); CPU->F &= ~(SUBTRACT_FLAG | CARRY_FLAG); CPU->F |= HALF_CARRY_FLAG; break;
    case 5: CPU->A ^= OPCode2; setFlag(CPU, ZERO_FLAG, CPU->A == 0); CPU->F &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG | CARRY_FLAG); break;
    case 6: CPU->A |= OPCode2; setFlag(CPU, ZERO_FLAG, CPU->A == 0); CPU->F &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG | CARRY_FLAG); break;
    case 7: resolveFlags(CPU, ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG | CARRY_FLAG, CPU->A, CPU->A - OPCode2, 0); break;
    }
}

void pushToStack(SM83* CPU, uint16_t val) {
    CPU->SP -= 2;
    writeMemoryWord(CPU->GB, CPU->SP, val);
}

uint16_t popFromStack(SM83* CPU) {
    uint16_t val = readMemoryWord(CPU->GB, CPU->SP);
    CPU->SP += 2;
    return val;
}

void executeInstruction(SM83* CPU) {
    uint8_t OPCode = readMemoryByte(CPU->GB, CPU->PC++);

    switch ((OPCode & 0b11000000) >> 6) {
    case 0:
        executeOpcodeGroup0(CPU, OPCode);
        break;
    case 1:
        executeOpcodeGroup1(CPU, OPCode);
        break;
    case 2:
        executeOpcodeGroup2(CPU, OPCode);
        break;
    case 3:
        executeOpcodeGroup3(CPU, OPCode);
        break;
    }
}

void executeOpcodeGroup0(SM83* CPU, uint8_t OPCode) {
    if ((OPCode & 0b00000111) == 0) {
        if ((OPCode & 0b00100000) == 0) {
            executeGroup0Subgroup0(CPU, OPCode);
        }
        else {
            jumpRelativeConditional(CPU, OPCode);
        }
    }
    else {
        if ((OPCode & 0b00000100) == 0) {
            executeGroup0Subgroup1(CPU, OPCode);
        }
        else {
            executeGroup0Subgroup2(CPU, OPCode);
        }
    }
}

void executeGroup0Subgroup0(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00011000) >> 3) {
    case 0:
        break;
    case 1:
        loadMemoryAddressWithSP(CPU);
        break;
    case 2:
        CPU->isStopped = true;
        break;
    case 3:
        jumpRelative(CPU);
        break;
    }
}

void executeGroup0Subgroup1(SM83* CPU, uint8_t OPCode) {
    switch (OPCode & 0x0F) {
    case 0x01:
        loadRegisterPairImmediate(CPU, OPCode);
        break;
    case 0x09:
        addHLWithRegisterPair(CPU, OPCode);
        break;
    case 0x02:
        loadMemoryWithA(CPU, OPCode);
        break;
    case 0x0A:
        loadAWithMemory(CPU, OPCode);
        break;
    case 0x03:
        incrementRegisterPair(CPU, OPCode);
        break;
    case 0x0B:
        decrementRegisterPair(CPU, OPCode);
        break;
    }
}

void executeGroup0Subgroup2(SM83* CPU, uint8_t OPCode) {
    switch (OPCode & 0b00000011) {
    case 0:
        incrementRegister(CPU, OPCode);
        break;
    case 1:
        decrementRegister(CPU, OPCode);
        break;
    case 2:
        loadRegisterImmediate(CPU, OPCode);
        break;
    case 3:
        executeSpecialOperation(CPU, OPCode);
        break;
    }
}

void jumpRelative(SM83* CPU) {
    int8_t displacement = readMemoryByte(CPU->GB, CPU->PC++);
    CPU->currentCycles += 4;
    CPU->PC += displacement;
}

void jumpRelativeConditional(SM83* CPU, uint8_t OPCode) {
    int8_t displacement = readMemoryByte(CPU->GB, CPU->PC++);
    if (evalCondition(CPU, OPCode)) {
        CPU->currentCycles += 4;
        CPU->PC += displacement;
    }
}

void loadRegisterPairImmediate(SM83* CPU, uint8_t OPCode) {
    uint16_t value = readMemoryWord(CPU->GB, CPU->PC);
    CPU->PC += 2;
    *getRegisterPointer16(CPU, OPCode) = value;
}

void addHLWithRegisterPair(SM83* CPU, uint8_t OPCode) {
    CPU->currentCycles += 4;
    uint16_t prevHL = CPU->HL;
    uint16_t regValue = *getRegisterPointer16(CPU, OPCode);
    CPU->HL += regValue;
    resolveFlags(CPU, HALF_CARRY_FLAG | CARRY_FLAG, (prevHL >> 8) & 0xFF, CPU->H,
        (prevHL & 0x00FF) > CPU->L);
}

void loadMemoryWithA(SM83* CPU, uint8_t OPCode) {
    uint16_t address = getAddressWithIncrementOrDecrement(CPU, OPCode);
    writeMemoryByte(CPU->GB, address, CPU->A);
}

void loadAWithMemory(SM83* CPU, uint8_t OPCode) {
    uint16_t address = getAddressWithIncrementOrDecrement(CPU, OPCode);
    CPU->A = readMemoryByte(CPU->GB, address);
}

void incrementRegisterPair(SM83* CPU, uint8_t OPCode) {
    CPU->currentCycles += 4;
    (*getRegisterPointer16(CPU, OPCode))++;
}

void decrementRegisterPair(SM83* CPU, uint8_t OPCode) {
    CPU->currentCycles += 4;
    (*getRegisterPointer16(CPU, OPCode))--;
}

void incrementRegister(SM83* CPU, uint8_t OPCode) {
    uint8_t* reg = getDestinationRegisterPointer8(CPU, OPCode);
    uint8_t preValue, postValue;
    if (reg) {
        preValue = *reg;
        (*reg)++;
        postValue = *reg;
    }
    else {
        preValue = readMemoryByte(CPU->GB, CPU->HL);
        postValue = preValue + 1;
        writeMemoryByte(CPU->GB, CPU->HL, postValue);
    }
    resolveFlags(CPU, ZERO_FLAG | HALF_CARRY_FLAG, preValue, postValue, 0);
}

void decrementRegister(SM83* CPU, uint8_t OPCode) {
    uint8_t* reg = getDestinationRegisterPointer8(CPU, OPCode);
    uint8_t preValue, postValue;
    if (reg) {
        preValue = *reg;
        (*reg)--;
        postValue = *reg;
    }
    else {
        preValue = readMemoryByte(CPU->GB, CPU->HL);
        postValue = preValue - 1;
        writeMemoryByte(CPU->GB, CPU->HL, postValue);
    }
    resolveFlags(CPU, ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG, preValue, postValue, 0);
}

void loadRegisterImmediate(SM83* CPU, uint8_t OPCode) {
    uint8_t value = readMemoryByte(CPU->GB, CPU->PC++);
    uint8_t* reg = getDestinationRegisterPointer8(CPU, OPCode);
    if (reg) {
        *reg = value;
    }
    else {
        writeMemoryByte(CPU->GB, CPU->HL, value);
    }
}

void executeSpecialOperation(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00111000) >> 3) {
    case 0:
        rotateLeftCarryA(CPU);
        break;
    case 1:
        rotateRightCarryA(CPU);
        break;
    case 2:
        rotateLeftA(CPU);
        break;
    case 3:
        rotateRightA(CPU);
        break;
    case 4:
        decimalAdjustA(CPU);
        break;
    case 5:
        complementA(CPU);
        break;
    case 6:
        setCarryFlag(CPU);
        break;
    case 7:
        complementCarryFlag(CPU);
        break;
    }
}

void rotateLeftCarryA(SM83* CPU) {
    CPU->F &= ~(ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG);
    setFlag(CPU, CARRY_FLAG, CPU->A & 0x80);
    CPU->A = (CPU->A << 1) | ((CPU->A & 0x80) >> 7);
}

void rotateRightCarryA(SM83* CPU) {
    CPU->F &= ~(ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG);
    setFlag(CPU, CARRY_FLAG, CPU->A & 0x01);
    CPU->A = (CPU->A >> 1) | ((CPU->A & 0x01) << 7);
}

void rotateLeftA(SM83* CPU) {
    CPU->F &= ~(ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG);
    uint8_t carry = (CPU->F & CARRY_FLAG) ? 1 : 0;
    setFlag(CPU, CARRY_FLAG, CPU->A & 0x80);
    CPU->A = (CPU->A << 1) | carry;
}

void rotateRightA(SM83* CPU) {
    CPU->F &= ~(ZERO_FLAG | SUBTRACT_FLAG | HALF_CARRY_FLAG);
    uint8_t carry = (CPU->F & CARRY_FLAG) ? 0x80 : 0;
    setFlag(CPU, CARRY_FLAG, CPU->A & 0x01);
    CPU->A = (CPU->A >> 1) | carry;
}

void decimalAdjustA(SM83* CPU) {
    uint8_t correction = 0x00;
    if (CPU->F & SUBTRACT_FLAG) {
        if (CPU->F & HALF_CARRY_FLAG) correction |= 0x06;
        if (CPU->F & CARRY_FLAG) correction |= 0x60;
        CPU->A -= correction;
    }
    else {
        if ((CPU->F & HALF_CARRY_FLAG) || (CPU->A & 0x0F) > 0x09) correction |= 0x06;
        if ((CPU->F & CARRY_FLAG) || CPU->A > 0x99) {
            correction |= 0x60;
            CPU->F |= CARRY_FLAG;
        }
        CPU->A += correction;
    }
    CPU->F &= ~HALF_CARRY_FLAG;
    setFlag(CPU, ZERO_FLAG, CPU->A == 0);
}

void complementA(SM83* CPU) {
    CPU->A = ~CPU->A;
    CPU->F |= SUBTRACT_FLAG | HALF_CARRY_FLAG;
}

void setCarryFlag(SM83* CPU) {
    CPU->F &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG);
    CPU->F |= CARRY_FLAG;
}

void complementCarryFlag(SM83* CPU) {
    CPU->F &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG);
    CPU->F ^= CARRY_FLAG;
}

void executeOpcodeGroup1(SM83* CPU, uint8_t OPCode) {
    if (OPCode == 0x76) {
        CPU->isHalted = true;
    }
    else {
        uint8_t* destReg = getDestinationRegisterPointer8(CPU, OPCode);
        uint8_t value = getSourceRegisterValue8(CPU, OPCode);
        if (destReg) {
            *destReg = value;
        }
        else {
            writeMemoryByte(CPU->GB, CPU->HL, value);
        }
    }
}

void executeOpcodeGroup2(SM83* CPU, uint8_t OPCode) {
    uint8_t value = getSourceRegisterValue8(CPU, OPCode);
    executeALUOperation(CPU, OPCode, value);
}

void executeOpcodeGroup3(SM83* CPU, uint8_t OPCode) {
    switch (OPCode & 0b00000111) {
    case 0:
        if ((OPCode & 0b00100000) == 0) {
            returnConditional(CPU, OPCode);
        }
        else {
            executeGroup3Specials0(CPU, OPCode);
        }
        break;
    case 1:
        if ((OPCode & 0b00001000) == 0) {
            popRegisterPair(CPU, OPCode);
        }
        else {
            executeGroup3Specials1(CPU, OPCode);
        }
        break;
    case 2:
        if ((OPCode & 0b00100000) == 0) {
            jumpConditional(CPU, OPCode);
        }
        else {
            executeGroup3Specials2(CPU, OPCode);
        }
        break;
    case 3:
        executeGroup3Specials3(CPU, OPCode);
        break;
    case 4:
        callConditional(CPU, OPCode);
        break;
    case 5:
        if ((OPCode & 0b00001000) == 0) {
            pushRegisterPair(CPU, OPCode);
        }
        else {
            callImmediate(CPU);
        }
        break;
    case 6:
        aluImmediate(CPU, OPCode);
        break;
    case 7:
        restart(CPU, OPCode);
        break;
    }
}

void returnConditional(SM83* CPU, uint8_t OPCode) {
    CPU->currentCycles += 4;
    if (evalCondition(CPU, OPCode)) {
        CPU->currentCycles += 4;
        CPU->PC = popFromStack(CPU);
    }
}

void popRegisterPair(SM83* CPU, uint8_t OPCode) {
    *getStackPointer16(CPU, OPCode) = popFromStack(CPU);
    CPU->F &= 0xF0;
}

void jumpConditional(SM83* CPU, uint8_t OPCode) {
    uint16_t address = readMemoryWord(CPU->GB, CPU->PC);
    CPU->PC += 2;
    if (evalCondition(CPU, OPCode)) {
        CPU->currentCycles += 4;
        CPU->PC = address;
    }
}

void executeGroup3Specials0(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00011000) >> 3) {
    case 0: {
        uint8_t n = readMemoryByte(CPU->GB, CPU->PC++);
        writeMemoryByte(CPU->GB, 0xFF00 + n, CPU->A);
        break;
    }
    case 1:
        addSPImmediate(CPU);
        break;
    case 2: {
        uint8_t num = readMemoryByte(CPU->GB, CPU->PC++);
        CPU->A = readMemoryByte(CPU->GB, 0xFF00 + num);
        break;
    }
    case 3:
        loadHLWithSPDisplacement(CPU);
        break;
    }
}

void addSPImmediate(SM83* CPU) {
    int8_t displacement = readMemoryByte(CPU->GB, CPU->PC++);
    uint16_t preSP = CPU->SP;
    CPU->currentCycles += 8;
    CPU->SP += displacement;
    CPU->F &= ~ZERO_FLAG;
    resolveFlags(CPU, HALF_CARRY_FLAG | CARRY_FLAG, preSP & 0x00FF,
        CPU->SP & 0x00FF, 0);
}

void loadHLWithSPDisplacement(SM83* CPU) {
    int8_t displacement = readMemoryByte(CPU->GB, CPU->PC++);
    CPU->currentCycles += 4;
    CPU->HL = CPU->SP + displacement;
    CPU->F &= ~ZERO_FLAG;
    resolveFlags(CPU, HALF_CARRY_FLAG | CARRY_FLAG, CPU->SP & 0x00FF,
        CPU->L, 0);
}

void executeGroup3Specials1(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00110000) >> 4) {
    case 0:
        CPU->currentCycles += 4;
        CPU->PC = popFromStack(CPU);
        break;
    case 1:
        CPU->currentCycles += 4;
        CPU->PC = popFromStack(CPU);
        CPU->IME = true;
        break;
    case 2:
        CPU->PC = CPU->HL;
        break;
    case 3:
        CPU->currentCycles += 4;
        CPU->SP = CPU->HL;
        break;
    }
}

void executeGroup3Specials2(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00011000) >> 3) {
    case 0: {
        writeMemoryByte(CPU->GB, 0xFF00 + CPU->C, CPU->A);
        break;
    }
    case 1: {
        uint16_t address = readMemoryWord(CPU->GB, CPU->PC);
        CPU->PC += 2;
        writeMemoryByte(CPU->GB, address, CPU->A);
        break;
    }
    case 2: {
        CPU->A = readMemoryByte(CPU->GB, 0xFF00 + CPU->C);
        break;
    }
    case 3: {
        uint16_t addr = readMemoryWord(CPU->GB, CPU->PC);
        CPU->PC += 2;
        CPU->A = readMemoryByte(CPU->GB, addr);
        break;
    }
    }
}

void executeGroup3Specials3(SM83* CPU, uint8_t OPCode) {
    switch ((OPCode & 0b00111000) >> 3) {
    case 0: {
        uint16_t address = readMemoryWord(CPU->GB, CPU->PC);
        CPU->PC = address;
        CPU->currentCycles += 4;
        break;
    }
    case 1:
        executePrefixCB(CPU);
        break;
    case 6:
        CPU->IME = false;
        break;
    case 7:
        CPU->ei = true;
        break;
    default:
        CPU->illegalOpcode = true;
        break;
    }
}

void executePrefixCB(SM83* CPU) {
    uint8_t cbCode = readMemoryByte(CPU->GB, CPU->PC++);
    uint8_t value = getSourceRegisterValue8(CPU, cbCode);
    uint8_t* destReg = getDestinationRegisterPointer8(CPU, cbCode << 3);
    uint8_t bit = (cbCode & 0b00111000) >> 3;
    int store = 1;
    int carryFlag;

    switch ((cbCode & 0b11000000) >> 6) {
    case 0:
        CPU->F &= ~(SUBTRACT_FLAG | HALF_CARRY_FLAG);
        switch (bit) {
        case 0:
            setFlag(CPU, CARRY_FLAG, value & 0x80);
            value = (value << 1) | ((value & 0x80) >> 7);
            break;
        case 1:
            setFlag(CPU, CARRY_FLAG, value & 0x01);
            value = (value >> 1) | ((value & 0x01) << 7);
            break;
        case 2:
            carryFlag = (CPU->F & CARRY_FLAG) ? 1 : 0;
            setFlag(CPU, CARRY_FLAG, value & 0x80);
            value = (value << 1) | carryFlag;
            break;
        case 3:
            carryFlag = (CPU->F & CARRY_FLAG) ? 0x80 : 0;
            setFlag(CPU, CARRY_FLAG, value & 0x01);
            value = (value >> 1) | carryFlag;
            break;
        case 4:
            setFlag(CPU, CARRY_FLAG, value & 0x80);
            value <<= 1;
            break;
        case 5:
            setFlag(CPU, CARRY_FLAG, value & 0x01);
            value = (value & 0x80) | (value >> 1);
            break;
        case 6:
            value = (value >> 4) | (value << 4);
            CPU->F &= ~CARRY_FLAG;
            break;
        case 7:
            setFlag(CPU, CARRY_FLAG, value & 0x01);
            value >>= 1;
            break;
        }
        setFlag(CPU, ZERO_FLAG, value == 0);
        break;
    case 1:
        store = 0;
        CPU->F &= ~SUBTRACT_FLAG;
        CPU->F |= HALF_CARRY_FLAG;
        setFlag(CPU, ZERO_FLAG, !(value & (1 << bit)));
        break;
    case 2:
        value &= ~(1 << bit);
        break;
    case 3:
        value |= (1 << bit);
        break;
    }
    if (store) {
        if (destReg) *destReg = value;
        else writeMemoryByte(CPU->GB, CPU->HL, value);
    }
}

void callConditional(SM83* CPU, uint8_t OPCode) {
    uint16_t address = readMemoryWord(CPU->GB, CPU->PC);
    CPU->PC += 2;
    if (evalCondition(CPU, OPCode)) {
        CPU->currentCycles += 4;
        pushToStack(CPU, CPU->PC);
        CPU->PC = address;
    }
}

void pushRegisterPair(SM83* CPU, uint8_t OPCode) {
    CPU->currentCycles += 4;
    pushToStack(CPU, *getStackPointer16(CPU, OPCode));
}

void callImmediate(SM83* CPU) {
    uint16_t address = readMemoryWord(CPU->GB, CPU->PC);
    CPU->PC += 2;
    pushToStack(CPU, CPU->PC);
    CPU->currentCycles += 4;
    CPU->PC = address;
}

void aluImmediate(SM83* CPU, uint8_t OPCode) {
    uint8_t value = readMemoryByte(CPU->GB, CPU->PC++);
    executeALUOperation(CPU, OPCode, value);
}

void restart(SM83* CPU, uint8_t OPCode) {
    pushToStack(CPU, CPU->PC);
    CPU->currentCycles += 4;
    CPU->PC = OPCode & 0b00111000;
}

void loadMemoryAddressWithSP(SM83* CPU) {
    uint16_t addr = readMemoryWord(CPU->GB, CPU->PC);
    CPU->PC += 2;
    writeMemoryWord(CPU->GB, addr, CPU->SP);
}

void CPUClock(SM83* CPU) {
    if (CPU->illegalOpcode) return;
    if (CPU->currentCycles == 0 && (CPU->GB->IE & CPU->GB->io[IF])) {
        CPU->isHalted = false;
        if (CPU->GB->io[IF] & INTERRUPT_JOYPAD) CPU->isStopped = false;
        if (CPU->IME) {
            CPU->currentCycles += 8;
            CPU->IME = false;
            int i;
            for (i = 0; i < 5; i++) {
                if ((CPU->GB->IE & CPU->GB->io[IF]) & (1 << i))
                    break;
            }
            if (i < 5) {
                CPU->GB->io[IF] &= ~(1 << i);
                pushToStack(CPU, CPU->PC);
                CPU->PC = 0b01000000 | (i << 3);
            }
        }
    }
    if (CPU->ei) {
        CPU->IME = true;
        CPU->ei = false;
    }
    if (CPU->currentCycles == 0 && !CPU->isHalted && !CPU->isStopped) {
        executeInstruction(CPU);
    }
    if (CPU->currentCycles) CPU->currentCycles--;
}
