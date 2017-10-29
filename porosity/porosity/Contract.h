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

class Contract {
public:
    Contract(bytes bytecode) {
        m_byteCode = bytecode;
        if (IsRuntimeCode(bytecode)) {
            m_byteCodeRuntime = bytecode;
        }
        else {
            // In the case IsRuntimeCode() didn't return the offset we move on.
            if (!m_runtimeOffset) return;

            bytes runtimeCode(bytecode.begin() + m_runtimeOffset, bytecode.end());
            m_byteCodeRuntime = runtimeCode;
        }

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

    bool
    IsRuntimeCode(
        bytes bytecode
    );

    string
    getGraphNodeColor(
        NodeType _type
    );

    bool
    addBlockReference(
        uint32_t _block,
        uint32_t _src,
        uint32_t _blockSize,
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

    auto
    addBasicBlock(
        uint32_t _offset,
        uint32_t _size
    ) -> map<unsigned int, _BasicBlockInfo>::iterator;

    uint32_t
    getInstructionIndexAtOffset(
        uint32_t _offset
    );

    bool
    addInstructionsToBlocks(
        void
    );

    uint32_t
    getBlockSize(
        uint32_t offset
    );

    void
    setBlockSize(
        uint32_t _offset,
        uint32_t _size
    );

    void
    forEachFunction(
        function<void(uint32_t)> const& _onFunction
    );

    void
    walkNodes(
        uint32_t _hash,
        uint32_t _block,
        std::function<bool(uint32_t, uint32_t, BasicBlockInfo *)> const& _onNode
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
    computeDominators(
        void
    );

    void
    printBranchName(
        uint32_t _offset
    );

    string
    getGraphviz(
        uint32_t _flag
    );

    void
    printFunctions(
            void
    );

    // Blocks
    void
    decompile(
        uint32_t _hash
    );

    bool
    StructureIfElse(
        BasicBlockInfo *_block
    );

    Statement
    StructureIfs(
        BasicBlockInfo *_block
    );

    uint32_t
    getBlockPredecessorsCount(
        BasicBlockInfo *_block
    );

    uint32_t
    getBlockSuccessorsCount(
        BasicBlockInfo *_block
    );

    BasicBlockInfo *
    getBlockAt(
        uint32_t _offset
    );

    void
    enumerateInstructionsAndBlocks(
        void
    );

    void
    assignXrefToBlocks(
        void
    );

    bool
    decompileBlock(
        SourceCode *decompiled_code,
        uint32_t _depth,
        BasicBlockInfo *_block
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
    uint32_t m_runtimeOffset = 0;
};
#endif