/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    VMState.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _VMSTATE_H_
#define _VMSTATE_H_

using namespace std;
using namespace dev;
using namespace dev::eth;

typedef enum _StackRegisterType {
    NumValueDefault = 0,
    Constant = 0x10,
    ConstantComputed = 0x11,
    RegTypeLabel = 0x11,
    RegTypeLabelCaller = 0x12,
    RegTypeLabelBlockHash = 0x13,
    RegTypeFlag = 0x20,

    RegTypeLabelSha3 = 0x30,
    UserInput = 0x100,
    UserInputTainted = 0x200,
    StorageType = 0x300
} StackRegisterType;

typedef struct _StackRegister {
    string name;
    string exp;
    uint32_t type; // StackRegisterType
    uint32_t offset;
    u256 value;
    ConditionAttribute cond;
} StackRegister;

#define MAX_MEMORY_SPACE 1024

#define IsConstant(first) (((first)->type == StackRegisterType::Constant) || ((first)->type == StackRegisterType::ConstantComputed))
#define IsStackEntryTypeTainted(type) (type & (UserInput | UserInputTainted))
#define IsMasking160bitsAddress(x) ((x)->value.compare(address_mask) == 0)

class VMState {
public:
    VMState() {
        clear();
    }

    bool
    executeInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data
    );

    void
    displayStack(
        void
    );

    inline void
    SwapStackRegisters(
        uint32_t index_a,
        uint32_t index_b
    );

    void
    pushStack(
        StackRegister data
    );

    void
    popStack(
        void
    );

    void
    setData(
        bytes _data
    );

    void
    setMemoryData(
        uint32_t _offset,
        StackRegister _data
    );

    StackRegister *
    getMemoryData(
        uint32_t _offset
    );

    string
    getDepth(
        void
    );

    void
    executeByteCode(
        bytes *_byteCodeRuntime
    );

    void
    clear() {
        m_memStorage.clear();
        m_stack.clear();

        // m_memStorage = dev::bytes(MAX_MEMORY_SPACE);
    }

    uint32_t m_eip;

private:
    bytes *m_byteCodeRuntimeRef;
    // uint32_t m_jmpFlag;

    vector<StackRegister> m_stack;
    bytes m_data;
    std::map<uint32_t, StackRegister> m_memStorage;
    u256 m_caller = 0xdeadbeef;

    int m_depthLevel = 1;
};


class InstructionContext {
public:
    InstructionContext(
        Instruction _instr,
        vector<StackRegister> _stack
    ) {
        m_instr = _instr;
        m_info = dev::eth::instructionInfo(m_instr);

        if (!dev::eth::isValidInstruction(_instr)) {
            printf("%02X                    !INVALID!", int(_instr));
            m_exp = "INVALID INSTRUCTION";
            return;
        }

        m_stack = _stack;
    }

    void
    printExpression() {
        if (m_exp.size()) printf("%s\n", m_exp.c_str());
    }

    bool
    getCurrentExpression(
        void
    );

    bool
    getContextForInstruction(
        void
    );

    string
    getDismangledRegisterName(
        StackRegister *first
    );


private:
    Instruction m_instr;
    InstructionInfo m_info;
    Statement m_stmt;
    string m_exp;

    vector<StackRegister> m_stack;

};

#endif