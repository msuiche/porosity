#ifndef _VMSTATE_H_
#define _VMSTATE_H_

using namespace std;
using namespace dev;
using namespace dev::eth;

#define MAX_MEMORY_SPACE 1024
class VMState {
public:
    typedef enum _ExpressionCmpOperator {
        ExpCmpOp_None = 0,
        ExpCmpOp_Equal = 0x1,
        ExpCmpOp_NonEqual = 0x2,
        ExpCmpOp_LessThan = 0x3,
        ExpCmpOp_GreaterThan = 0x4,
        ExpCmpOp_SignedLessThan = 0x5,
        ExpCmpOp_SignedGreaterThan = 0x6,
        ExpCmpOp_IsZero = 0x7
    } ExpressionCmpOperator;

    typedef struct _Expression {
        Instruction instr;
        string name;
        _Expression *_exp_ptr;
        ExpressionCmpOperator opr;
    } Expression;

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
        uint32_t type; // StackRegisterType
        uint32_t offset;
        u256 value;

        Expression exp; // mainly for conditional expressions.
    } StackRegister;

    VMState() {
        clear();
    }

    bool
    executeInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data
    );

    Expression
    getExpressionForInstruction(
        Instruction _instr,
        StackRegister *first,
        StackRegister *second,
        StackRegister *third
    );

    Expression
    getCurrentExpression(
        Instruction _instr
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
        uint16_t _offset,
        u256 _data
    );

    string
    getDepth(
        void
    );

    string
    getDismangledRegisterName(
        StackRegister *first
    );

    void
    executeByteCode(
        bytes *_byteCodeRuntime
    );

    void
    clear() {
        m_mem.clear();
        m_stack.clear();

        m_mem = dev::bytes(MAX_MEMORY_SPACE);
    }

    bytes *m_byteCodeRuntimeRef;
    // uint32_t m_jmpFlag;

    vector<StackRegister> m_stack;
    bytes m_data;
    bytes m_mem;
    uint32_t m_eip;
    u256 m_caller = 0xdeadbeef;

    int m_depthLevel = 1;
};

#endif