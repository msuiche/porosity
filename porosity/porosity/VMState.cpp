/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    VMState.cpp

Abstract:
    Porosity Emulator and Abstraction layer.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/

#include "Porosity.h"

using namespace std;
using namespace dev;
using namespace dev::eth;

u256 address_mask("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
u256 endianMul("0x1000000000000000000000000");

#define TaintStackEntry(x) (m_stack[x].type |= StackRegisterType::UserInputTainted)
#define TagStackEntryAsConstant(x) (m_stack[x].type |= StackRegisterType::Constant)

#define GetStackEntryById(x) m_stack[x]
#define IsStackEntryTainted(x) (m_stack[x].type & (UserInput | UserInputTainted))
#define IsStackEntryTypeTainted(type) (type & (UserInput | UserInputTainted))
#define IsMasking160bitsAddress(x) ((x)->value.compare(address_mask) == 0)

#define IsStackIndexPresent(x) (m_stack.size() > x)

inline void 
VMState::SwapStackRegisters(
    uint32_t index_a,
    uint32_t index_b
)
{
    if (IsStackIndexPresent(index_a) && IsStackIndexPresent(index_b)) {
        StackRegister oldValue = m_stack[index_a];
        StackRegister newValue = m_stack[index_b];

        m_stack[index_b] = oldValue;
        m_stack[index_a] = newValue;
    }
}

std::string random_string(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

string
VMState::getDepth(
    void
)
{
    stringstream tabs;
    for (int i = 0; i < m_depthLevel; i+= 1) tabs << "    ";
    return tabs.str();
}

void
displayStack(
    vector<StackRegister> *stack
) {
    int regIndex = 0;

    for (auto it = stack->begin(); it != stack->end(); ++it) {
        u256 data = it->value;
        printf("%08X: ", regIndex);
        std::cout << std::setfill('0') << std::setw(64) << std::hex << data;

        printf(" ");
        printf("%s", it->type & UserInput ? "U" : "-");
        printf("%s", it->type & UserInputTainted ? "T" : "-");
        printf("%s", it->type & Constant ? "C" : "-");
        printf("%s", it->type & ConstantComputed ? "X" : "-");
        printf("%s", it->type & RegTypeLabelCaller ? "L" : "-");
        printf("%s", it->type & RegTypeLabelBlockHash ? "B" : "-");
        printf("%s", it->type & RegTypeLabelSha3 ? "3" : "-");
        printf("%s", it->type & StorageType ? "S" : "-");
        printf("%s", it->type & RegTypeFlag ? "F" : "-");

        printf(" [name = %s]", it->name.c_str());
        printf(" [exp = %s]", it->exp.c_str());
        printf("\n");

        regIndex++;
    }
}

void
VMState::displayStack(
    void
) {
    printf("EIP: 0x%x STACK: \n", m_eip);

    ::displayStack(&m_stack);
}

void
VMState::pushStack(
    StackRegister data
) {
    if (data.name.empty()) data.name = "var_" + random_string(5);
    m_stack.insert(m_stack.begin(), data);
}

void
VMState::popStack(
    void
)
{
    if (m_stack.size()) {
        m_stack.erase(m_stack.begin());
    }
    else {
        printf("ERROR: Stack Underflow.\n");
    }
}

bool
VMState::executeInstruction(
    uint32_t _offset,
    Instruction _instr,
    u256 const& _data,
    bool permission2Fork
) {
    InstructionInfo info = dev::eth::instructionInfo(_instr);

    if (!dev::eth::isValidInstruction(_instr)) {
        printf("%02X                    !INVALID!", int(_instr));
        return false;
    }

    switch (_instr) {
        case Instruction::PUSH1:
        case Instruction::PUSH2:
        case Instruction::PUSH3:
        case Instruction::PUSH4:
        case Instruction::PUSH5:
        case Instruction::PUSH6:
        case Instruction::PUSH7:
        case Instruction::PUSH8:
        case Instruction::PUSH9:
        case Instruction::PUSH10:
        case Instruction::PUSH11:
        case Instruction::PUSH12:
        case Instruction::PUSH13:
        case Instruction::PUSH14:
        case Instruction::PUSH15:
        case Instruction::PUSH16:
        case Instruction::PUSH17:
        case Instruction::PUSH18:
        case Instruction::PUSH19:
        case Instruction::PUSH20:
        case Instruction::PUSH21:
        case Instruction::PUSH22:
        case Instruction::PUSH23:
        case Instruction::PUSH24:
        case Instruction::PUSH25:
        case Instruction::PUSH26:
        case Instruction::PUSH27:
        case Instruction::PUSH28:
        case Instruction::PUSH29:
        case Instruction::PUSH30:
        case Instruction::PUSH31:
        case Instruction::PUSH32:
        {
            StackRegister reg = { "", "", Constant, 0, 0 };
            reg.value = _data;
            reg.type = Constant;
            pushStack(reg);
            break;
        }
        case Instruction::CALLDATALOAD:
        {
            // https://github.com/ethereum/cpp-ethereum/blob/develop/libevm/VM.cpp#L659
            // 0x0 -> 0x04 (magic code associated to function hash)
            // e.g. 0xcdcd77c000000000000000000000000000000000000000000000000000000000 /( 2^0xe0) -> 0xcdcd77c0
            // argv[0]: 0x04 -> 0x24 - 256bits (32 bytes)
            // argv[1]: 0x24 -> 0x44 - 256bits (32 bytes)
            // m_stack.erase(m_stack.begin());
            StackRegister reg = { "", "", UserInput, 0, 0 };

            reg.type = UserInput;
            uint32_t offset = int(m_stack[0].value);
            reg.offset = offset;

            if (m_data.size()) {
                if (offset + 31 < m_data.size())
                    reg.value = (u256)*(h256 const*)(m_data.data() + (size_t)offset);
                else if (reg.value >= m_data.size())
                    reg.value = u256(0); // invalid
            }
            else {
                //
                // In case the caller didn't provide any input parameters
                //
            }

            stringstream argname;
            argname << "arg_";
            argname << std::hex << offset;
            reg.name = argname.str();
            m_stack[0] = reg;
            break;
        }
        
        case Instruction::CALLDATASIZE:
        {
            StackRegister reg = { "", "", UserInput, 0, 0 };
            reg.value = m_data.size();
            reg.type = UserInput;
            pushStack(reg);
            break;
        }
        
        case Instruction::MSTORE:
        {
            uint32_t offset = int(GetStackEntryById(0).value);
            setMemoryData(offset, GetStackEntryById(1));
            stringstream argname;

            if (GetStackEntryById(1).type & StorageType) {
                argname << GetStackEntryById(1).name;
            }
            else {
                argname << "memory[0x";
                argname << std::hex << offset;
                argname << "]";
            }
            popStack();
            popStack();

            m_stack[0].name = argname.str();
        }
        break;
        case Instruction::SSTORE:
            // TODO:
            popStack();
            popStack();
        break;
        case Instruction::MLOAD:
        {
            uint32_t offset = int(GetStackEntryById(0).value);
            StackRegister *reg = getMemoryData(offset);
            stringstream argname;
            if (reg && reg->type & StorageType) {
                GetStackEntryById(0).name = reg->name;
            }
            else {
                argname << "memory[0x";
                argname << std::hex << offset;
                argname << "]";
                GetStackEntryById(0).name = argname.str();
            }
        }
        break;
        case Instruction::SLOAD:
        {
            stringstream argname;
            uint32_t offset = int(m_stack[0].value);
            argname << "store_";
            argname << std::hex << offset;

            if (GetStackEntryById(0).type == RegTypeLabelSha3)
                GetStackEntryById(0).name = "store[" + GetStackEntryById(0).name + "]";
            else
                GetStackEntryById(0).name = argname.str();

            GetStackEntryById(0).type = StorageType;
            break;
        }
        case Instruction::SHA3:
        {
            uint64_t offset = (uint64_t)m_stack[0].value;
            uint64_t size = (uint64_t)m_stack[1].value;
            // stack[0] = sha3(memStorage + offset, size);
            popStack();
            // popStack();
            //GetStackEntryById(0).value = u256(dev::keccak256(GetStackEntryById(0).value));
            GetStackEntryById(0) = *getMemoryData(offset);
            m_stack[0].type = RegTypeLabelSha3;
            break;
        }
        case Instruction::BALANCE:
        {
            StackRegister reg = { "", "", ConstantComputed, 0, 0 };
            reg.value = 10000000;
            reg.type = ConstantComputed;
            pushStack(reg);

            break;
        }
        case Instruction::CALL:
        {
            if (m_stack.size() < 6) {
                printf("*** ERROR *** STACK UNDERFLOW (%lu elements < 6)\n", m_stack.size());
                return false;
            }
            // Stack[0] = gas limit (default = 0x2540B5EF0)
            // Stack[1] = caller (e.g. msg.sender)
            // Stack[2] = value
            // Stack[3]
            // Stack[4]
            // Stack[5]

            stringstream exp;
            string value = "";
            int gasLimit = int(GetStackEntryById(0).value);
            int callType = int(GetStackEntryById(1).value);

            if (GetStackEntryById(1).type & RegTypeLabelCaller) {
                exp << GetStackEntryById(1).name;
            }
            else {
                switch (callType) {
                case Sha256Type:
                    exp << "sha256";
                    value = GetStackEntryById(3).name;

                    //uint32_t offset = int(GetStackEntryById(3).value);
                    //StackRegister *reg = getMemoryData(offset);
                    //reg->name = "sha256_value";
                    break;
                case RipeMd160Type:
                    exp << "ripemd160";
                    value = GetStackEntryById(3).name;
                    //uint32_t offset = int(GetStackEntryById(3).value);
                    //StackRegister *reg = getMemoryData(offset);
                    //reg->name = "ripemd160_value";
                    break;
                default:
                    exp << std::hex << GetStackEntryById(1).value;
                    value = GetStackEntryById(2).name;
                    break;
                }
            }
            exp << ".call";
            if (gasLimit != 0x2540B5EF0) exp << ".gas(" << gasLimit << ")";
            exp << ".value(" << value << ")";
            exp << "()";

            popStack();
            popStack();
            popStack();
            popStack();
            popStack();
            popStack();

            GetStackEntryById(0).exp = exp.str();
            GetStackEntryById(0).type = CallReturnStatus;
            switch (callType) {
                case Sha256Type:
                    GetStackEntryById(0).name = "resultSha256";
                    GetStackEntryById(0).value = true;
                    break;
                case RipeMd160Type:
                    GetStackEntryById(0).name = "resultRMD160";
                    GetStackEntryById(0).value = true;
                    break;
                default:
                    GetStackEntryById(0).name = "result";
                    GetStackEntryById(0).value = false; // always failing scenario.
                break;
            }
            break;
        }
        case Instruction::LOG0:
        case Instruction::LOG1:
        case Instruction::LOG2:
        case Instruction::LOG3:
        case Instruction::LOG4:
        {
            popStack();
            popStack();
            int itemsToPop = int(_instr) - int(Instruction::LOG0);
            for (int i = 0; i < itemsToPop; i++) popStack();
            break;
        }
        case Instruction::DUP1:
        case Instruction::DUP2:
        case Instruction::DUP3:
        case Instruction::DUP4:
        case Instruction::DUP5:
        case Instruction::DUP6:
        case Instruction::DUP7:
        case Instruction::DUP8:
        case Instruction::DUP9:
        case Instruction::DUP10:
        case Instruction::DUP11:
        case Instruction::DUP12:
        case Instruction::DUP13:
        case Instruction::DUP14:
        case Instruction::DUP15:
        case Instruction::DUP16:
        {
            uint8_t index = int(_instr) - int(Instruction::DUP1);
            pushStack(m_stack[index]);
            break;
        }
        case Instruction::SWAP1:
        case Instruction::SWAP2:
        case Instruction::SWAP3:
        case Instruction::SWAP4:
        case Instruction::SWAP5:
        case Instruction::SWAP6:
        case Instruction::SWAP7:
        case Instruction::SWAP8:
        case Instruction::SWAP9:
        case Instruction::SWAP10:
        case Instruction::SWAP11:
        case Instruction::SWAP12:
        case Instruction::SWAP13:
        case Instruction::SWAP14:
        case Instruction::SWAP15:
        case Instruction::SWAP16:
        {
            uint8_t index = int(_instr) - (int(Instruction::SWAP1) - 1);

            // printf("SWAP(stack[0], stack[%d])\n", index);
            StackRegister oldValue = m_stack[index];
            StackRegister newValue = m_stack[0];

            m_stack[0] = oldValue;
            m_stack[index] = newValue;
            break;
        }
        case Instruction::POP:
            popStack();
        break;
        case Instruction::SUB:
            m_stack[0].value = m_stack[0].value - m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack();
            break;
        case Instruction::DIV:
            if (!m_stack[1].value) m_stack[0].value = 0;
            else m_stack[0].value = m_stack[0].value / m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack();
            break;
        case Instruction::ADD:
            m_stack[0].value = m_stack[0].value + m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack();
            break;
        case Instruction::MUL:
            //u256 endianMul("0x1000000000000000000000000");
            //if ((m_stack[1].value.compare(endianMul) == 0)
            if (m_stack[0].value.compare(endianMul) == 0) {
                // mask for address. type discovery.
                m_stack[0].type = m_stack[1].type; // copy mask to result.
                                                   // m_stack[0].type &= AddressType;
                m_stack[0].name = m_stack[1].name;
            }

            m_stack[0].value = m_stack[0].value * m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack();
            break;
        break;
        case Instruction::MOD:
            m_stack[0].value = m_stack[0].value % m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack();
            break;
        case Instruction::EXP:
            m_stack[0].value = porosity::exp256(m_stack[0].value, m_stack[1].value);
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            // if (IsConstant(&GetStackEntryById(0))) GetStackEntryById(0).type = ConstantComputed;
            m_stack[1] = m_stack[0];
            popStack();
            break;
        case Instruction::AND:
        {
            if (IsMasking160bitsAddress(&GetStackEntryById(0))) {
                // mask for address. type discovery.
                m_stack[0].type = m_stack[1].type; // copy mask to result.
                // m_stack[0].type &= AddressType;
                m_stack[0].name = m_stack[1].name;
            }

            m_stack[0].value = m_stack[0].value & m_stack[1].value;
            // if (IsStackEntryTainted(1)) TaintStackEntry(0);
            m_stack[1] = m_stack[0];
            popStack(); // info.ret
            break;
        }
        case Instruction::NOT:
        {
            m_stack[0].value = ~m_stack[0].value;
            break;
        }
        case Instruction::OR:
        {
            m_stack[0].value = m_stack[0].value | m_stack[1].value;
            m_stack[1] = m_stack[0];
            popStack(); // info.ret
            break;
        }
        case Instruction::EQ:
            // change name
            // change labeltype
            // tag expression to value stack.
            m_stack[1].type = RegTypeFlag;
            m_stack[1].value = (m_stack[1].value == m_stack[0].value);
            m_stack[1].exp = m_stack[0].exp;
            // m_jmpFlag = int(m_stack[1].value);
            popStack();
            break;
        case Instruction::LT:
            // change name
            // change labeltype
            // tag expression to value stack.
            m_stack[1].type = RegTypeFlag;
            m_stack[1].value = (m_stack[0].value < m_stack[1].value);
            m_stack[1].exp = m_stack[0].exp;
            // m_jmpFlag = int(m_stack[1].value);
            popStack();
            break;
        case Instruction::GT:
            // change name
            // change labeltype
            // tag expression to value stack.
            m_stack[1].type = RegTypeFlag;
            m_stack[1].value = (m_stack[0].value > m_stack[1].value);
            m_stack[1].exp = m_stack[0].exp;
            // m_jmpFlag = int(m_stack[1].value);
            popStack();
            break;
        case Instruction::GAS:
        {
            StackRegister reg = { "", "", GasLimit, 0, 0 };
            reg.value = 0x2540be3f2;
            reg.type = GasLimit;
            reg.name = "gaslimit";
            pushStack(reg);
            break;
        }
        case Instruction::ISZERO:
        {
            m_stack[0].value = (m_stack[0].value == 0);
            m_stack[0].type = RegTypeFlag;
            break;
        }
        case Instruction::JUMP:
            // printf("JUMP: stack.size() = 0x%x\n", m_stack.size());
            // printf("JUMP: target = 0x%x\n", int(m_stack[0].value));
            if (m_stack.size()) {
                m_eip = int(m_stack[0].value);
                popStack();
            }
            return true;
        break;
        case Instruction::JUMPI:
        {
            int jmpTarget = int(m_stack[0].value);
            int nextInstr = (m_eip + sizeof(Instruction) + info.additional);
            int cond = int(m_stack[1].value);

            popStack();
            popStack();

            if (permission2Fork) {
                if (cond) {
                    if (g_VerboseLevel > 4) printf("**** FORK BEGIN****\n");
                    // We force the opposite condition for discovery.
                    // requires additional coverage.
                    VMState state = *this;
                    state.m_depthLevel++;
                    bytes subBlock(m_byteCodeRuntimeRef->begin(), m_byteCodeRuntimeRef->end());
                    state.m_eip = nextInstr;
                    state.executeByteCode(&subBlock);

                    // Resume execution.
                    if (g_VerboseLevel > 4) printf("**** FORK END****\n");
                    m_eip = jmpTarget;
                    return true;
                }
                else {
                    if (g_VerboseLevel > 4) printf("**** FORK BEGIN****\n");
                    // We force the opposite condition for discovery.
                    // requires additional coverage.
                    VMState state = *this;
                    state.m_depthLevel++;
                    bytes subBlock(m_byteCodeRuntimeRef->begin(), m_byteCodeRuntimeRef->end());
                    state.m_eip = jmpTarget;
                    state.executeByteCode(&subBlock);

                    // resume execution.
                    if (g_VerboseLevel > 4) printf("**** FORK END****\n");
                    m_eip = nextInstr;
                    return true;
                }
            }
            break;
        }
        case Instruction::JUMPDEST:
            // new basic block
            break;
        case Instruction::INVALID:
        case Instruction::STOP:
        case Instruction::SUICIDE:
            // done
            return false;
        break;
        case Instruction::RETURN:
            return false;
        break;
        case Instruction::PC:
        {
            stringstream argname;
            argname << "loc_";
            argname << std::hex << m_eip;

            m_stack[0].type = RegTypeLabel;
            m_stack[0].name = argname.str();
            m_stack[0].value = m_eip;
            break;
        }
        case Instruction::CALLER:
        {
            // this can send actions, check if in between brackets.
            u256 data = _data;
            StackRegister reg = { "", "", RegTypeLabelCaller, 0, 0 };
            reg.value = m_caller;
            reg.name = "msg.sender";
            reg.type = RegTypeLabelCaller;
            pushStack(reg);
            break;
        }
        case Instruction::BLOCKHASH:
        {
            u256 data = _data;
            StackRegister reg = { "", "", RegTypeLabelBlockHash, 0, 0 };
            reg.value = u256("0xdeadbeefdeadbeefdeadbeefdeadbeef");
            reg.name = "blockhash";
            reg.type = RegTypeLabelBlockHash;
            pushStack(reg);
            break;
        }
        case Instruction::CALLVALUE:
        {
            StackRegister reg = { "msg.value", "", StackRegisterType::UserInputTainted, 0, 0 };
            reg.value = u256("0x0bad1dea0bad1dea0bad1dea0bad1dea");
            pushStack(reg);
            break;
        }
        case Instruction::ADDRESS:
        {
            // this can send actions, check if in between brackets.
            u256 data = _data;
            StackRegister reg = { "", "", AddressType, 0, 0 };
            reg.value = m_caller;
            reg.name = "this";
            reg.type = AddressType | StackRegisterType::UserInputTainted;
            pushStack(reg);
            break;
        }
        
        default:
            printf("%s: NOT_IMPLEMENTED: %s\n", __FUNCTION__, info.name.c_str());
            return false;
            break;
    }

    if (g_VerboseLevel >= 4) displayStack();
    if (m_stack.size()) m_stack[0].lastModificationPC = m_eip;
    m_eip += sizeof(Instruction) + info.additional;

    return true;
}

void
VMState::setData(
    bytes _data
)
{
    m_data = _data;
}

void
VMState::setMemoryData(
    uint32_t _offset,
    StackRegister _data
) {
    auto it = m_memStorage.find(_offset);

    if (it == m_memStorage.end()) {
        m_memStorage.insert(m_memStorage.end(), pair<uint32_t, StackRegister>(_offset, _data));
    }
    else {
#if g_VerboseLevel > 6
        printf("overwritting memory entry.\n");
#endif
        it->second = _data;
    }

}

StackRegister *
VMState::getMemoryData(
    uint32_t _offset
) {
    auto it = m_memStorage.find(_offset);

    if (it != m_memStorage.end()) return &it->second;

    return 0;
}

bool
VMState::isEndOfBlock(
    Instruction _instr
) {

    switch (_instr) {
        case Instruction::JUMP:
        case Instruction::JUMPI:
        case Instruction::SUICIDE:
        case Instruction::RETURN:
        case Instruction::STOP:
        case Instruction::INVALID:
            return true;
        break;
            
        default:
            return false;
        break;
    }

    return false;
}

bool
VMState::executeBlock(
    BasicBlockInfo *_block
) {
    bool result = true;
    // if (_block->visited) return false;

    // VMState current = *this;

    m_eip = _block->offset;
    if (g_VerboseLevel >= 3) {
        printf("%s: block(id = %d, offset = 0x%x, size = 0x%d)\n", __FUNCTION__, _block->id, _block->offset, _block->size);
        for (uint32_t i = 0; i < 32; i++) {
            if (_block->dominators & (1 << i)) {
                printf("%s: dominators: ", __FUNCTION__);
                if (_block->dominators & (1 << i)) {
                    printf("%d", i);
                }
                printf("\n");
            }
        }
    }

    for (auto opcde = _block->instructions.begin(); opcde != _block->instructions.end(); ++opcde) {

        // resolve expression
        InstructionContext instrCxt(opcde->offInfo.inst, m_stack);
        instrCxt.getCurrentExpression();
        if (g_SingleStepping || (g_VerboseLevel >= 2)) porosity::printInstruction(opcde->offInfo.offset, opcde->offInfo.inst, opcde->offInfo.data);
        opcde->stack = instrCxt.m_stack; // make sure to save the stack for each instruction
        m_stack = opcde->stack;

        if (isEndOfBlock(opcde->offInfo.inst)) {
            if (opcde->offInfo.inst == Instruction::JUMP) {
                // printf("Execute basic block\n");
                if (!_block->dstDefault) {
                    result = false;
                    break;
                }

                if ((opcde->stack.size() && (opcde->stack[0].value != _block->dstDefault)) || (_block->dstDefault == int(NODE_DEADEND))) {
                    uint32_t newDest = int(opcde->stack[0].value);
                    if (g_VerboseLevel >= 2) printf("ERR: Invalid destionation. (0x%08X -> 0x%08X)\n", _block->dstDefault, newDest);
                    _block->dstDefault = newDest;
                    _block->nextDefault = getBlockAt(newDest);
                }
            }
            else if (opcde->offInfo.inst == Instruction::JUMPI) {
                if (!_block->dstJUMPI) {
                    result = false;
                    break;
                }

                if ((opcde->stack.size() && opcde->stack[0].value != _block->dstJUMPI)) {
                    uint32_t newDest = int(opcde->stack[0].value);
                    if (g_VerboseLevel >= 2) printf("ERR: Invalid destionation. (0x%08X -> 0x%08X)\n", _block->dstJUMPI, newDest);
                    _block->dstJUMPI = newDest;
                    _block->nextJUMPI = getBlockAt(newDest);
                }
            }
            else {
                result = false;
                break;
            }
        }

        // Flags
        switch (opcde->offInfo.inst) {
            case Instruction::CALL:
            {
                _block->InheritFlags |= NoMoreSSTORE;
                break;
            }
                
            default:
                break;
        }

        bool ret = executeInstruction(opcde->offInfo.offset, opcde->offInfo.inst, opcde->offInfo.data, false);
    }

    _block->visited = true;

    return result;
}

void
VMState::executeFunction(
    BasicBlockInfo *_entrypoint
) {
    // Execute current block and the branches.
    BasicBlockInfo *block = _entrypoint;
    if (!block) return;

    while (true) {
        if (!executeBlock(block)) break;
        uint32_t inheritFlags = block->InheritFlags;

        if (block->dstJUMPI) {
            BasicBlockInfo *next = block->nextJUMPI;
            if (next) {
                next->Flags |= inheritFlags;
                next->InheritFlags |= inheritFlags;

                VMState state = *this;
                state.m_depthLevel++;
                state.m_eip = block->dstJUMPI;
                state.executeFunction(next);
            }
            else {
                printf("ERROR: JUMPI destination is null.\n");
            }
        }

        block = block->nextDefault;
        if (!block) break;
        block->Flags |= inheritFlags;
        block->InheritFlags |= inheritFlags;
    }
}

void
VMState::executeByteCode(
    bytes *_byteCodeRuntime
) {
    m_byteCodeRuntimeRef = _byteCodeRuntime;

    if (m_depthLevel > 4) return; // limit

    while (true)
    {
        auto it = _byteCodeRuntime->begin() + m_eip;
        Instruction instr = Instruction(*it);
        size_t additional = 0;
        if (isValidInstruction(instr)) additional = instructionInfo(instr).additional;

        uint32_t offset = std::distance(_byteCodeRuntime->begin(), it);
        u256 data;

        for (size_t i = 0; i < additional; ++i)
        {
            data <<= 8;
            if (++it < m_byteCodeRuntimeRef->end()) {
                data |= *it;
            }
        }


        if (g_SingleStepping) {
            printf("=================\n");
            printf("BEFORE:\n");
            displayStack();
        }
        if (g_SingleStepping || (g_VerboseLevel >= 2)) porosity::printInstruction(offset, instr, data);

        InstructionContext instrCxt(instr, m_stack);
        if (instrCxt.getCurrentExpression()) {
            instrCxt.printExpression();
        }

        bool ret = executeInstruction(offset, instr, data, true);
        if (g_SingleStepping) {
            printf("AFTER:\n");
            displayStack();
            printf("=================\n");
        }

        if (g_SingleStepping) getchar();

        if (!ret || (m_eip == m_byteCodeRuntimeRef->size())) break;
    }
}

// Instruction Context

bool
InstructionContext::getCurrentExpression(
    void
) {
    if (getContextForInstruction()) {
        if (m_exp.size()) {
            if (g_VerboseLevel >= 3) printf("%s: ", __FUNCTION__);
            if (g_VerboseLevel >= 2) printf("%s\n", m_exp.c_str());
        }
    }

    return true;
}

bool
InstructionContext::getContextForInstruction(
    void
) {
    string exp;

    Instruction instr = m_instr;
    StackRegister *first = 0;
    if (m_stack.size() > 0) first = &m_stack[0];
    StackRegister *second = 0;
    if (m_stack.size() > 1) second = &m_stack[1];
    StackRegister *third = 0;
    if (m_stack.size() > 2) third = &m_stack[2];

    switch (m_instr) {
        case Instruction::CALLDATALOAD:
        {
            if (g_VerboseLevel >= 6) {
                stringstream argname;
                uint32_t offset = int(first->value);
                argname << "arg_";
                argname << std::hex << offset;

                exp = first->name + " = " + argname.str() + ";";
            }
            break;
        }
        case Instruction::NOT:
        {
            if (!IsConstant(first))
                exp = first->name + " = ~" + getDismangledRegisterName(first) + ";";
            break;
        }
        case Instruction::AND:
        {
            if (!IsConstant(first) && !IsMasking160bitsAddress(first) && !IsMasking160bitsAddress(first))
                exp = first->name + " &= " + getDismangledRegisterName(first) + ";";
            break;
        }
        case Instruction::ADD:
        case Instruction::MUL:
        case Instruction::SUB:
        case Instruction::DIV:
        case Instruction::SDIV:
        case Instruction::MOD:
        case Instruction::SMOD:
        case Instruction::EXP:
        {
            const char *operation[] = { "+", "*", "-", "/", "/", "%%", "%%", "invld", "invld", "**", 0 };
            int index = int(instr) - int(Instruction::ADD);
            /*exp = first->name + " = " + getDismangledRegisterName(first) + " "
            + operation[index] + " " + getDismangledRegisterName(second) + ";";*/

            if (g_VerboseLevel >= 6) {
                exp = first->name + " " + operation[index] + "= " + getDismangledRegisterName(second) + ";";
            }
            else {
                if (!IsConstant(first)) {
                    exp = first->name + " " + operation[index] + "= " + getDismangledRegisterName(second) + ";";
                }
            }

            string op;

            if ((first->value.compare(endianMul) == 0) && (IsStackEntryTypeTainted(second->type))) {
                op = getDismangledRegisterName(second);
            }
            else if ((second->value.compare(endianMul) == 0) && (IsStackEntryTypeTainted(first->type))) {
                op = getDismangledRegisterName(first);
            }
            else {
                op = getDismangledRegisterName(first) + " " + operation[index] + " " + getDismangledRegisterName(second);
            }
            first->exp = op;

            break;
        }
        case Instruction::LT:
        case Instruction::GT:
        case Instruction::SLT:
        case Instruction::SGT:
        case Instruction::EQ:
        {
            const char *operation[] = { "<", ">", "<", ">", "==", 0 };
            stringstream mod;
            bool isSigned = ((instr == Instruction::SLT) || (instr == Instruction::SGT));
            string signedParam = isSigned ? "(signed)" : "";

            int index = int(instr) - int(Instruction::LT);
            exp = "(" + signedParam + getDismangledRegisterName(first) + " " + operation[index] + " " + signedParam + getDismangledRegisterName(second) + ")";

            first->exp = exp;
            // first->instr = instr;
            // m_stmt.setCondition(instr);

            if (g_VerboseLevel >= 6) {
                exp = first->name;
            }
            else {
                exp = "";
            }
            break;
        }
        case Instruction::ISZERO:
        {
            // change name
            // change labeltype
            // tag expression to value stack.
            // m_jmpFlag = (m_stack[0].value == 0);

            string cond;
            if (first) {
                if (first->exp.size()) cond = first->exp;
                else cond = getDismangledRegisterName(first);
                // exp = "(!(" + cond + "))";
            }
            exp = cond;

            // m_stmt.setCondition(instr);

            first->exp = exp;
            if (g_VerboseLevel >= 6) {
                exp = first->exp;
            }
            else {
                exp = "";
            }

            break;
        }
        case Instruction::CALLVALUE:
        {
            // exp = "this";
            break;
        }
        case Instruction::JUMPDEST:
        {
            // TODO: If not function header;
            // exp = "}";
            break;
        }
        case Instruction::JUMPI:
        {
            exp = "if (!" + second->exp + ") {";
            break;
        }
        case Instruction::ADDMOD:
        case Instruction::MULMOD:
        {
            const char *operation[] = { "+", "*", 0 };
            int index = int(instr) - int(Instruction::ADDMOD);
            exp = first->name + " = (" + getDismangledRegisterName(first) + " " + operation[index];
            exp += " " + getDismangledRegisterName(second) + ") %% " + getDismangledRegisterName(third) + ";";
            break;
        }
        case Instruction::SSTORE:
        {
            stringstream argname;
            string var_name;

            uint32_t offset = int(first->value);
            argname << "store_";
            argname << std::hex << offset;

            if (first->type == RegTypeLabelSha3)
                var_name = "store[" + first->name + "]";
            else
                var_name = argname.str();

            if ((g_VerboseLevel > 4) || (var_name != getDismangledRegisterName(second)))
                exp = var_name + " = " + getDismangledRegisterName(second) + ";";
            break;
        }
        case Instruction::MSTORE:
        {
            stringstream argname;
            uint32_t offset = int(first->value);
            argname << "memory[0x";
            argname << std::hex << offset;
            argname << "]";

    #if (g_VerboseLevel >= 6)
            exp = argname.str() + " = " + getDismangledRegisterName(second) + ";";
    #endif
            break;
        }
        case Instruction::SLOAD:
        {
    #if (g_VerboseLevel >= 6)
            stringstream argname;
            uint32_t offset = int(first->value);
            argname << "store_";
            argname << std::hex << offset;

            if (first->type == RegTypeLabelSha3)
                exp = "store[" + first->name + "]";
            else
                exp = argname.str();
    #endif
            break;
        }
        case Instruction::SHA3:
        {
            uint64_t offset = int(first->value);
            uint64_t size = int(second->value);
            // stack[0] = sha3(memStorage + offset, size);

    #if (g_VerboseLevel >= 6)
            {
                //uint64_t offset = (uint64_t)first->value;
                //uint64_t size = (uint64_t)second->value;
                exp = "sha3(" + getDismangledRegisterName(getMemoryData(offset)) + ", " + getDismangledRegisterName(second) + ");";
            }
    #endif
            break;
        }
        case Instruction::LOG0:
        case Instruction::LOG1:
        case Instruction::LOG2:
        case Instruction::LOG3:
        case Instruction::LOG4:
        {
            // Events allow light clients to react on changes efficiently.

            // Sent(msg.sender, receiver, amount);
            // LOG1(m_stack[0], m_stack[1], m_stack[2])

            // exp = "LOG(" + getDismangledRegisterName(getMemoryData(int(first->value))) + ", " + getDismangledRegisterName(getMemoryData(int(second->value)));
            exp = "LOG(" + getDismangledRegisterName(first) + ", " + getDismangledRegisterName(second);
            int itemsToPop = int(instr) - int(Instruction::LOG0);
            for (int i = 0; i < itemsToPop; i++) exp += ", " + getDismangledRegisterName(&GetStackEntryById(2 + i));
            exp += ");";

            break;
        }
        case Instruction::STOP:
            exp = "return;";
            break;
        case Instruction::RETURN:
            exp = "return " + first->name + ";";
            break;
        default:
            return false;
            break;
    }

    if (exp.size()) m_exp = exp;

    return true;
}

string
InstructionContext::getDismangledRegisterName(
    StackRegister *first
) {
    if (!first) return "invalid";

    switch (first->type) {
    case UserInput:
    case UserInputTainted:
    case StorageType:

        return first->name;
        break;
    case RegTypeLabelSha3:
        return "store[" + first->name + "]";
    case RegTypeFlag:
        // TODO: point to Expresion *
        break;
    case ConstantComputed:
    case Constant:
    {
        stringstream mod;
        mod << "0x" << std::hex << first->value;
        return mod.str();
        // mod << "0x" << std::hex << second->value;
        break;
    }
    case RegTypeLabelCaller:
        return "msg.sender";
        break;
    case RegTypeLabelBlockHash:
        return "blockhash";
        break;
    default:
        // return first->name;
        break;
    }

    if (g_VerboseLevel >= 5) printf("%s: unsupported type (type = 0x%x).\n", __FUNCTION__, first->type);
    return first->name;
}
