/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    BasicBlock.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _BASICBLOCK_H_
#define _BASICBLOCK_H_

#include "Instruction.h"

using namespace std;
using namespace dev;
using namespace dev::eth;

#define MAX_XREFS 32
#define NODE_DEADEND 0xdeadbabe

#define DEPTH(x) for (uint32_t i = 0; i < x; i++) printf("   ");

// Enums

typedef enum _DErrCode {

    DCode_OK = 0,

    DCode_Warn = 0x1000,

    DCode_Err = 0x2000,
    DCode_Err_ReentrantVulnerablity = 0x2001
} DErrCode;

typedef enum _StatementName {
    StatementUndefined = 0x0,
    StatementIf = 0x1,
    StatementElse = 0x2,
    StatementWhile = 0x3,
    StatementFor = 0x4
} StatementName;

typedef enum _ConditionAttribute {
    ConditionUndefined = 0x0,

    ConditionEqual = 0x1,
    ConditionDifferent = 0x2,
    ConditionIsZero = 0x3,
    ConditionIsNotZero = 0x4,

    ConditionLessThan = 0x5,
    ConditionGreaterEqualThan = 0x6,
    ConditionGreaterThan = 0x7,
    ConditionLessEqualThan = 0x8,

    ConditionSignedLessThan = 0x9,
    ConditionSignedGreaterEqualThan = 0x10,
    ConditionSignedGreaterThan = 0x11,
    ConditionSignedGreatedLessEqualThan = 0x12

} ConditionAttribute;

typedef enum _NodeType {
    RegularNode = 0x0,
    ConditionalNode = 0x1,
    ExitNode = 0x2
} NodeType;

typedef enum _CallFunctionType {
    Sha256Type = 2,
    RipeMd160Type = 3
} CallFunctionType;

typedef enum _StackRegisterType {
    NumValueDefault = 0,
    Constant = (1 << 1),
    ConstantComputed = (1 << 2),
    RegTypeLabel = (1 << 3),
    RegTypeLabelCaller = (1 << 4),
    RegTypeLabelBlockHash = (1 << 5),
    RegTypeFlag = (1 << 6),

    RegTypeLabelSha3 = (1 << 7),
    UserInput = (1 << 8),
    UserInputTainted = (1 << 9),
    StorageType = (1 << 10),

    GasLimit = (1 << 11),
    CallReturnStatus = (1 << 12),
    AddressType = (1 << 13),
    RegTypeLabelThis = (1 << 14)
} StackRegisterType;

// Structures


typedef struct _FunctionDef {
    std::string name;
    std::string abiName;
    std::string inputs;
    std::string type;
} FunctionDef;

typedef struct _OffsetInfo {
    uint32_t offset;
    dev::eth::Instruction inst;
    dev::eth::InstructionInfo instInfo;
    dev::u256 data;
} OffsetInfo;

typedef struct _StackRegister {
    string name;
    string exp;
    uint32_t type; // StackRegisterType
    uint32_t offset;

    uint32_t lastModificationPC;
    u256 value;
    ConditionAttribute cond;
} StackRegister;

typedef struct _InstructionState {
    OffsetInfo offInfo;
    vector<StackRegister> stack;
} InstructionState;

typedef struct _Xref {
    uint32_t offset;
    NodeType conditional;
} Xref;

typedef enum _BlockFlags {
    NoMoreSSTORE = (1 << 1)
} BlockFlags;

typedef struct _BasicBlockInfo
{
    uint32_t id;
    uint32_t fnAddrHash; // Head
    std::map<uint32_t, Xref> references; // map<uint32_t, NodeType>

    _BasicBlockInfo *nextDefault;
    _BasicBlockInfo *nextJUMPI;

    uint32_t dstDefault;
    uint32_t dstJUMPI;


    uint32_t hashtag;
    uint32_t offset;
    uint32_t size;
    string name;

    bool visited;
    bool walkedNode;
    u256 dominators;

    ConditionAttribute condAttr;
    vector<InstructionState> instructions;

    uint32_t Flags;
    uint32_t InheritFlags;

} BasicBlockInfo;


#define MAX_MEMORY_SPACE 1024

#define IsConstant(first) (((first)->type == StackRegisterType::Constant) || ((first)->type == StackRegisterType::ConstantComputed))
#define IsStackEntryTypeTainted(type) (type & (UserInput | UserInputTainted))
#define IsMasking160bitsAddress(x) ((x)->value.compare(address_mask) == 0)

#endif