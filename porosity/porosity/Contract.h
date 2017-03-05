/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Contract.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _CONTRACT_H_
#define _CONTRACT_H_

using namespace std;
using namespace dev;
using namespace dev::eth;

#define MAX_XREFS 32

#define NODE_DEADEND 0xdeadbabe

typedef enum _NodeType {
    RegularNode = 0x0,
    ConditionalNode = 0x1,
    ExitNode = 0x2
} NodeType;

typedef struct _Xref {
    uint32_t offset;
    NodeType conditional;
} Xref;

typedef struct _BasicBlockInfo
{
    uint32_t fnAddrHash; // Head
    std::map<uint32_t, Xref> references; // map<uint32_t, NodeType>
    uint32_t dstDefault;
    uint32_t dstJUMPI;
    uint32_t hashtag;
    string name;
} BasicBlockInfo;

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

class Contract {
public:
    Contract(bytes bytecode) {
        m_byteCodeRuntime = bytecode;

        getBasicBlocks();
    }

    std::string
    printInstructions(
        void
    );

    void
    getBasicBlocks(
        void
    );

    string
    getGraphNodeColor(
        NodeType _type
    );

    bool
    addBlockReference(
        uint32_t _block,
        uint32_t _src,
        uint32_t _fnAddrHash,
        NodeType _conditional
    );

    bool
    tagBasicBlock(
        uint32_t dest,
        string name
    );

    void
    walkAndConnectNodes(
        uint32_t _hash,
        uint32_t _block
    );

    bool
    tagBasicBlockWithHashtag(
        uint32_t dest,
        uint32_t hash
    );

    void
    printBlockReferences(
        void
    );

    string
    resolveBranchName(
        uint32_t offset
    );

    void
    setABI(
        string abiFile,
        string abi
    );

    string
    getFunctionName(
        uint32_t hash
    );

    void
    getFunction(
        uint32_t hash
    );

    uint32_t
    getFunctionOffset(
        uint32_t hash
    );

    void
    forEachFunction(
        function<void(uint32_t)> const& _onFunction
    );

    void
    printInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data
    );

    void
    setData(
        bytes _data
    );

    void
    executeByteCode(
        VMState *_vmState,
        bytes *_byteCodeRuntime,
        size_t _byteCodeRuntimeLimit
    );

    void
    printBranchName(
        uint32_t _offset
    );

    string
    getGraphviz(
        void
    );

    void
    printFunctions(
            void
    );

private:

    map<uint32_t, uint32_t> m_exitNodesByHash;
    std::map<uint32_t, BasicBlockInfo> m_listbasicBlockInfo;
    std::map<uint32_t, FunctionDef> m_publicFunctions;
    std::vector<OffsetInfo> m_instructions;

    VMState m_vmState;

    nlohmann::json m_abi_json;
    bytes m_byteCode;
    bytes m_byteCodeRuntime;
};
#endif