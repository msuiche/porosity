/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Contract.cpp

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/

#include "Porosity.h"

using namespace std;
using namespace dev;
using namespace dev::eth;

void
Contract::printBranchName(
    uint32_t _offset
) {
    string name = resolveBranchName(_offset);
    if (name.size()) printf("\n%s:\n", name.c_str());
}

string
Contract::printInstructions(
)
{
    stringstream ret;

    if (m_byteCodeRuntime.empty()) return "";

    printf("- Total byte code size: 0x%x (%d)\n\n", m_byteCodeRuntime.size(), m_byteCodeRuntime.size());

    dev::eth::eachInstruction(m_byteCodeRuntime, [&](uint32_t _offset, Instruction _instr, u256 const& _data) {
        printBranchName(_offset);
        porosity::printInstruction(_offset, _instr, _data);
    });

    return ret.str();
}

auto
Contract::addBasicBlock(
    uint32_t _offset,
    uint32_t _size
) {
    BasicBlockInfo newEntry = { 0 };
    newEntry.offset = _offset;
    newEntry.size = _size;

    auto newBlock = m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(_offset, newEntry));

    return newBlock;
}

void
Contract::enumerateInstructionsAndBlocks(
    void
) {
    m_instructions.clear();
    m_listbasicBlockInfo.clear();

    addBasicBlock(NODE_DEADEND, 0); // ExitNode to be resolved later.
    addBasicBlock(0x0, 0); // EntryPoint

    // Collect all the basic blocks.
    dev::eth::eachInstruction(m_byteCodeRuntime, [&](uint32_t _offset, Instruction _instr, u256 const& _data) {

        //
        // Indexing each instruction, to be able to retro execute instructions.
        //
        InstructionInfo info = dev::eth::instructionInfo(_instr);
        OffsetInfo offsetInfo;
        offsetInfo.offset = _offset;
        offsetInfo.instInfo = info;
        offsetInfo.inst = _instr;
        offsetInfo.data = _data;
        m_instructions.insert(m_instructions.end(), offsetInfo);

        //
        // Collecting the list of basic blocks.
        //
        if (_instr == Instruction::JUMPDEST) {
            addBasicBlock(_offset, 0);
        }
    });
}

void
Contract::assignXrefToBlocks(
    void
)
{
    
    //
    // Attributing the XREF
    //

    uint32_t basicBlockOffset = 0;

    for (uint32_t instIndex = 0; instIndex < m_instructions.size(); instIndex++) {
        OffsetInfo *currentOffInfo = &m_instructions[instIndex];
        uint32_t currentOffset = currentOffInfo->offset;

        uint32_t currentEndOffset;
        if ((instIndex + 1) < m_instructions.size()) {
            OffsetInfo *next = &m_instructions[instIndex + 1];
            currentEndOffset = next->offset;
        }
        else {
            currentEndOffset = m_byteCodeRuntime.size();
        }
        uint32_t currentBlockSize = currentOffset - basicBlockOffset;
        uint32_t nextInstrBlockSize = currentEndOffset - basicBlockOffset;

        // basicBlockOffset offCurrent->offset;

        switch (currentOffInfo->inst) {
            case Instruction::JUMPDEST:
            {
                uint32_t prevBasicBlockOffset = basicBlockOffset;
                basicBlockOffset = currentOffset;
                // uint32_t currentBlockSize = basicBlockOffset - prevBasicBlockOffset;

                auto it = m_listbasicBlockInfo.find(currentOffset);
                // it->second.size = currentBlockSize;
                bool isPublicFunction = false;

                if (it != m_listbasicBlockInfo.end()) {
                    isPublicFunction = (it->second.fnAddrHash != 0);
                }

                if (isPublicFunction && ((instIndex + 1) < m_instructions.size())) {
                    // The "RETURN" basic block address is pushed at the beginning of functions.
                    // assert (m_listbasicBlockInfo.find(basicBlockOffset)->second.abiName)
                    OffsetInfo *next = &m_instructions[instIndex + 1];
                    if ((next->inst == Instruction::PUSH1) ||
                        (next->inst == Instruction::PUSH2)) {
                        u256 data = next->data;
                        uint32_t jmpDest = int(data);
                        //
                        // addBlockReference((uint32_t)jmpDest, basicBlockOffset, false, ExitNode);
                        // tagBasicBlockWithHashtag(jmpDest, it->second.fnAddrHash);
                        m_exitNodesByHash.insert(m_exitNodesByHash.begin(), pair<uint32_t, uint32_t>(it->second.fnAddrHash, jmpDest));
                    }
                }
                else if (instIndex > 0) {
                    //auto prevInst = instruction;
                    //--prevInst;
                    OffsetInfo *prev = &m_instructions[instIndex - 1];
                    // check if current block is the beginning of a new basic block.
                    // but how did we get there ?
                    if ((prev->inst != Instruction::STOP) &&
                        (prev->inst != Instruction::RETURN) &&
                        (prev->inst != Instruction::SUICIDE) &&
                        (prev->inst != Instruction::JUMP) &&
                        (prev->inst != Instruction::JUMPI) &&
                        (prev->inst != Instruction::JUMPC) &&
                        (prev->inst != Instruction::JUMPCI)) {
                        // some gap created due to optmization to fill.
                        addBlockReference((uint32_t)basicBlockOffset, prevBasicBlockOffset, currentBlockSize, false, RegularNode);
                    }
                }
                break;
            }
            case Instruction::STOP:
            {
                tagBasicBlock(basicBlockOffset, "STOP");
                setBlockSize(basicBlockOffset, nextInstrBlockSize);
                break;
            }
            case Instruction::RETURN:
            {
                tagBasicBlock(basicBlockOffset, "RETURN");
                setBlockSize(basicBlockOffset, nextInstrBlockSize);
                break;
            }
            case Instruction::JUMPI:
            {
                uint32_t fnAddrHash = 0;
                if (instIndex >= 1) {
                    OffsetInfo *prev = &m_instructions[instIndex - 1];

                    if ((prev->inst == Instruction::PUSH1) ||
                        (prev->inst == Instruction::PUSH2)) {
                        u256 data = prev->data;
                        uint32_t jmpDest = int(data);

                        for (uint32_t j = 0; (instIndex >= j) && (j < 5); j += 1) {
                            OffsetInfo *offFnHash = &m_instructions[instIndex - j];

                            if (offFnHash->inst == Instruction::PUSH4) {
                                u256 data = offFnHash->data;
                                fnAddrHash = int(data & 0xFFFFFFFF);
                            }
                        }

                        addBlockReference((uint32_t)jmpDest, basicBlockOffset, nextInstrBlockSize, fnAddrHash, ConditionalNode);
                    }
                    if (g_VerboseLevel >= 3) printf("%s: function @ 0x%08X (hash = 0x%08x)\n",
                        __FUNCTION__, basicBlockOffset, fnAddrHash);
                }

                if ((instIndex + 1) < m_instructions.size()) {
                    OffsetInfo *next = &m_instructions[instIndex + 1];
                    // next basic block.
                    uint32_t prevBasicBlockOffset = basicBlockOffset;
                    basicBlockOffset = next->offset;
                    if (next->inst != Instruction::JUMPDEST) {
                        // We need to add this new basic block.
                        // printf("JUMPDEST: 0x%08X\n", _offset);
                        addBasicBlock(next->offset, nextInstrBlockSize);
                    }
                    addBlockReference(next->offset, prevBasicBlockOffset, nextInstrBlockSize, false, RegularNode);
                }
                break;
            }
            case Instruction::JUMP:
            {
                if ((instIndex > 0)) {
                    OffsetInfo *prev = &m_instructions[instIndex - 1];

                    if ((prev->inst == Instruction::PUSH1) ||
                        (prev->inst == Instruction::PUSH2)) {
                        u256 data = prev->data;
                        uint32_t jmpDest = int(data);
                        addBlockReference((uint32_t)jmpDest, basicBlockOffset, nextInstrBlockSize, false, RegularNode);
                    }
                    else {
                        u256 data = prev->data;
                        uint32_t exitBlock = int(data);
                        addBlockReference(int(NODE_DEADEND), basicBlockOffset, nextInstrBlockSize, false, ExitNode);
                        // addBlockReference(exitBlock, basicBlockOffset, false, ExitNode);
                    }
                }
                if (g_VerboseLevel >= 3) printf("%s: branch @ 0x%08X\n", __FUNCTION__, basicBlockOffset);
                break;
            }
        }
    }

    //
    // Reconnect to exit nodes.
    //
    for (auto func = m_listbasicBlockInfo.begin(); func != m_listbasicBlockInfo.end(); ++func) {
        auto refs = func->second.references;
        uint32_t hash = func->second.fnAddrHash;
        if (!hash) continue;

        walkAndConnectNodes(hash, func->first);
    }

    m_listbasicBlockInfo.erase(m_listbasicBlockInfo.find(NODE_DEADEND));
}

void
Contract::getBasicBlocks(
    void
)
{
    if (m_byteCodeRuntime.empty()) return;

    enumerateInstructionsAndBlocks();

    assignXrefToBlocks();

    addInstructionsToBlocks();
    computeDominators();

    return;
}

void
Contract::computeDominators(
    void
) {
    int nBlocks = m_listbasicBlockInfo.size();
    uint32_t i = 0;

    for (auto block = m_listbasicBlockInfo.begin();
         block != m_listbasicBlockInfo.end();
         ++block) {
        block->second.id = i++;

        for (int j = 0; j < nBlocks; j++) block->second.dominators |= (1 << j);
    }

    m_listbasicBlockInfo[0].dominators = 0;
    m_listbasicBlockInfo[0].dominators |= (1 << m_listbasicBlockInfo[0].id);

    u256 T;

    bool changed = false;
    do {
        changed = false;

        for (auto block = m_listbasicBlockInfo.begin();
             block != m_listbasicBlockInfo.end();
              ++block) {
                if (block == m_listbasicBlockInfo.begin()) continue;

                for (auto pred = block->second.references.begin();
                        pred != block->second.references.end();
                        ++pred) {

                    T = 0;
                    T |= block->second.dominators;

                    BasicBlockInfo *predBlock = getBlockAt(pred->first);
                    if (!predBlock) break;
                    block->second.dominators &= predBlock->dominators;
                    block->second.dominators |= (1 << block->second.id);

                    if (block->second.dominators != T) changed = true;
                }

            }

    } while (changed);
}

bool
Contract::addInstructionsToBlocks(
    void
)
{
    for (auto block = m_listbasicBlockInfo.begin(); block != m_listbasicBlockInfo.end(); ++block) {
        uint32_t off_start = block->second.offset;
        uint32_t off_end = off_start + block->second.size;

        uint32_t index_start = getInstructionIndexAtOffset(off_start);
        uint32_t index_end = getInstructionIndexAtOffset(off_end);
        if (g_VerboseLevel > 2) printf("Instruction Copy (start = 0x%x, end = 0x%x)\n", off_start, off_end);
        while (index_start < index_end) {
            InstructionState instState;
            instState.offInfo = m_instructions[index_start];
            block->second.instructions.insert(block->second.instructions.end(), instState);
            index_start++;
        }
    }

    return true;
}

uint32_t
Contract::getInstructionIndexAtOffset(
    uint32_t _offset
) {
    uint32_t instIndex = 0;

    for (instIndex = 0; instIndex < m_instructions.size(); instIndex++) {
        OffsetInfo *currentOffInfo = &m_instructions[instIndex];
        uint32_t currentOffset = currentOffInfo->offset;
        if (currentOffset == _offset) break;
    }

    // if offset not found, we still return the number of instructions (last block)
    return instIndex;
}

void
Contract::walkAndConnectNodes(
    uint32_t _hash,
    uint32_t _block
)
{
    //
    uint32_t next = _block;
    while (true) {
        auto block = m_listbasicBlockInfo.find(next); // getBlockAt()
        if (block == m_listbasicBlockInfo.end()) break;

        if (block->second.walkedNode) break;

        next = block->second.dstDefault;

        // printf("hash = 0x%x, block = 0x%x, default = 0x%x, JUMPI = 0x%x\n", _hash, _block, block->second.dstDefault, block->second.dstJUMPI);
        if (block->second.dstJUMPI) {
            block->second.walkedNode = true;
            walkAndConnectNodes(_hash, block->second.dstJUMPI);
        }

        if (!next) break;
        if (next == NODE_DEADEND) {
            auto exitNode = m_exitNodesByHash.find(_hash); // getBlockAt()
            if (exitNode != m_exitNodesByHash.end()) {
                block->second.dstDefault = exitNode->second;
            }

            if (!block->second.nextDefault) {
                block->second.nextDefault = getBlockAt(block->second.dstDefault);
            }
            break; // next
        }
    }
}

void
Contract::walkNodes(
    uint32_t _hash,
    uint32_t _block,
    std::function<bool(uint32_t, uint32_t, BasicBlockInfo *)> const& _onNode
)
{
    // TODO: port walkAndConnectNodes()
    uint32_t next = _block;
    while (true) {
        BasicBlockInfo *block = getBlockAt(next);
        if (!block) break;

        uint32_t current = next;
        next = block->dstDefault;

        if (block->dstJUMPI) {
            walkNodes(_hash, block->dstJUMPI, _onNode);
        }

        if (!_onNode(_hash, current, block)) break;

        next = block->dstDefault;
        if (!next) break;
    }
}

bool
Contract::tagBasicBlock(
    uint32_t dest,
    string name
)
{
    auto it = m_listbasicBlockInfo.find(dest);
    if (it != m_listbasicBlockInfo.end()) {
        it->second.name = name;
        return true;
    }

    return false;
}

bool
Contract::tagBasicBlockWithHashtag(
    uint32_t _dest,
    uint32_t _hash
)
{
    auto it = m_listbasicBlockInfo.find(_dest);
    if (it != m_listbasicBlockInfo.end()) {
        it->second.hashtag = _hash;
        return true;
    }

    return false;
}

bool
Contract::addBlockReference(
    uint32_t _block,
    uint32_t _src,
    uint32_t _blockSize,
    uint32_t _fnAddrHash,
    NodeType _conditional
)
{
    bool srcRefAdded = false;
    bool dstRefAdded = false;

    //
    // Add references/source.
    //
   auto it = m_listbasicBlockInfo.find(_block);
   if (it != m_listbasicBlockInfo.end()) {
       Xref ref = { _src, _conditional };

       it->second.references.insert(it->second.references.begin(), pair<uint32_t, Xref>(_src, ref));
       it->second.fnAddrHash = _fnAddrHash;
       srcRefAdded = true;
   }

   //
   // Add destination
   //
   it = m_listbasicBlockInfo.find(_src);
   if (it != m_listbasicBlockInfo.end()) {

       if (_conditional == NodeType::ConditionalNode) {
           it->second.nextJUMPI = getBlockAt(_block);
           it->second.dstJUMPI = _block;
       } else {
           it->second.nextDefault = getBlockAt(_block);
           it->second.dstDefault = _block;
       }
       it->second.size = _blockSize;
       dstRefAdded = true;
   }

   return (dstRefAdded && srcRefAdded);
}

string
Contract::resolveBranchName(
    uint32_t offset
) {
    std::stringstream stream;

    auto it = m_listbasicBlockInfo.find(offset);
    if (it != m_listbasicBlockInfo.end()) {
        if (it->second.fnAddrHash) {
            stream << getFunctionName(it->second.fnAddrHash);
        }
        else {
            stream << "loc_"
                << std::setfill('0') << std::setw(sizeof(uint32_t) * 2);
            stream << std::hex << offset;
        }
    }

    return stream.str();
}

uint32_t
Contract::getBlockSize(
    uint32_t offset
) {
    auto it = m_listbasicBlockInfo.find(offset);
    if (it != m_listbasicBlockInfo.end()) {
        return it->second.size;
    }

    return 0;
}


void
Contract::setBlockSize(
    uint32_t _offset,
    uint32_t _size
) {
    auto it = m_listbasicBlockInfo.find(_offset);
    if (it != m_listbasicBlockInfo.end()) {
        it->second.size = _size;
    }

    return;
}

void
Contract::printBlockReferences(
    void
)
{
    // DEBUG
    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        auto refs = it->second.references;
        printf("(dest = 0x%08X, numrefs = 0x%08X, refs = {", it->first, refs.size());
        for (auto ref = refs.begin(); ref != refs.end(); ++ref) {
            printf("0x%08x", ref->second.offset);
        }
        printf("}, ");
        if (it->second.dstJUMPI) printf("JUMPI: 0x%08X, ", it->second.dstJUMPI);
        printf("Default: 0x%08X ", it->second.dstDefault);
        if (it->second.fnAddrHash) {
            printf(", hash = 0x%08X, ", it->second.fnAddrHash);
            printf("str = %s, ", getFunctionName(it->second.fnAddrHash).c_str());
        }
        printf(")");
        printf("\n");
    }
}

string
Contract::getGraphNodeColor(
    NodeType _type
)
{
    switch (_type) {
        case ConditionalNode:
            return "dodgerblue";
        case ExitNode:
            return "red";
    }

    return "";
}

string
Contract::getGraphviz(
    uint32_t _flag
)
{
    // DEBUG
    if (g_VerboseLevel >= 4) printBlockReferences();

    string graph;

    graph = "digraph porosity {\n";
    graph += "rankdir = TB;\n";
    graph += "size = \"12\"\n";
    graph += "graph[fontname = Courier, fontsize = 10.0, labeljust = l, nojustify = true];";
    graph += "node[shape = record];\n";

    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        uint32_t source = it->first;
        string source_str = porosity::to_hstring(source);
        uint32_t dstIfTrue = it->second.dstJUMPI;
        string ifTrue_str = porosity::to_hstring(dstIfTrue);
        uint32_t dstDefault = it->second.dstDefault;
        string dstDefault_str = porosity::to_hstring(dstDefault);
        uint32_t symbolHash = it->second.fnAddrHash;
        string symbolName = symbolHash ? getFunctionName(symbolHash) : "loc_" + porosity::to_hstring(source);
        string defaultColor = dstIfTrue ? "red" : "black";
        uint32_t basicBlockSize = it->second.size;

        bytes subBlockCode(m_byteCodeRuntime.begin() + source, m_byteCodeRuntime.begin() + source + basicBlockSize);

        string basicBlockCode;
        if (_flag)
            basicBlockCode = porosity::buildNode(subBlockCode, source);
        else
            basicBlockCode = symbolName;

        graph += "    \"" + source_str + "\"" "[label = \"" + basicBlockCode + "\"];\n";
        if (dstDefault) {
            graph += "    \"" + source_str + "\"" + " -> " + "\"" + dstDefault_str + "\"" + " [color=\""+ defaultColor +"\"];\n";
        }
        if (dstIfTrue) {
            graph += "    \"" + source_str + "\"" + " -> " + "\"" + ifTrue_str + "\"" + " [color=\"green\"];\n";
        }
#if 0
        auto refs = it->second.references;

        if (refs.size()) {
            for (auto ref = refs.begin(); ref != refs.end(); ++ref) {
                uint32_t offset_str = ref->second.offset;
                NodeType nodeType = ref->second.conditional;
                uint32_t offset_dst = it->first;
                graph += "    ";
                if (!it->second.fnAddrHash) {
                    graph += "\"" + porosity::to_hstring(offset_str) + "\"" + " -> " + "\"" + porosity::to_hstring(offset_dst) + "\"";
                    graph += " [color=\"" + getGraphNodeColor(nodeType) + "\"]";
                    graph += ";";
                }
                else {
                    graph += "\"" + porosity::to_hstring(offset_str) + "\"" + " -> " + "\"" + porosity::to_hstring(offset_dst) + "\""
                        + "[label = \"" + getFunctionName(it->second.fnAddrHash).c_str() + "(" + porosity::to_hstring(it->second.fnAddrHash) + ")\"";
                    graph += " color=\"" + getGraphNodeColor(nodeType) + "\"";
                    graph += "]; ";
                }
                graph += "\n";
            }
        } else {
            graph += "    ";
            graph += "\"" + porosity::to_hstring(int(NODE_DEADEND)) + "\"" + " -> " + "\"" + porosity::to_hstring(it->first) + "\"" + "[label = \""  + +it->second.name.c_str() + "\"";
            graph += " color=black fillcolor = lightgray";
            graph += "]; ";
            graph += "\n";
        }
#endif
    }

    graph += "}\n";
    return graph;
}

void
Contract::setABI(
    string _abiFile,
    string _abi
) {
    string abi;

    if (_abiFile.empty() && !_abi.empty()) {
        abi = _abi;
    }
    else if (!_abiFile.empty() && _abi.empty()) {
        ifstream file(_abiFile);
        stringstream s;
        if (!file.is_open()) {
            printf("%s: Can't open file.\n", __FUNCTION__);
            return;
        }
        s << file.rdbuf();
        file.close();
        abi = s.str();
    }
    else {
        return;
    }

    m_publicFunctions.clear();

    printf("Attempting to parse ABI definition...\n");
    if (g_VerboseLevel >= 5)  printf("%s\n", abi.c_str());
    m_abi_json = nlohmann::json::parse(abi.c_str());
    printf("Success.\n");

    for (nlohmann::json entry : m_abi_json) {
        string name = "";
        string entry_type = entry["type"];
        if (entry_type != "function") continue;
        string entry_name = entry["name"];
        name += entry_name;
        name += "(";

        std::vector<string> parameters;

        for (nlohmann::json input : entry["inputs"]) {
            parameters.push_back(input["type"]);
        }

        for (size_t i = 0; i < parameters.size(); i += 1) {
            name += parameters[i];
            if ((i + 1) < parameters.size()) name += ",";
        }
        name += ")";

        string abiMethod = name;
        abiMethod.erase(std::remove(abiMethod.begin(), abiMethod.end(), '"'), abiMethod.end());
        if (g_VerboseLevel >= 5) printf("%s: Name: %s\n", __FUNCTION__, abiMethod.c_str());
        uint32_t hashMethod;

        dev::FixedHash<4> hash(dev::keccak256(abiMethod));
        hashMethod = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | hash[3];

        FunctionDef def;
        if (entry_type == "function") {
            string abiMethodName = entry_name;
            abiMethodName.erase(std::remove(abiMethodName.begin(), abiMethodName.end(), '"'), abiMethodName.end());
            def.name = abiMethodName;
            def.abiName = abiMethod;
            m_publicFunctions.insert(m_publicFunctions.begin(), pair<uint32_t, FunctionDef>(hashMethod, def));
        }
        if (g_VerboseLevel >= 5) printf("%s: signature: 0x%08x\n", __FUNCTION__, hashMethod);
    }
}

string
Contract::getFunctionName(
    uint32_t hash
) {
    auto it = m_publicFunctions.find(hash);
    if (it != m_publicFunctions.end()) {
        // return it->second.name;
        return it->second.abiName;
    }
    else {
        stringstream stream;

        stream << "func_"
            << std::setfill('0') << std::setw(sizeof(uint32_t) * 2);
        stream << std::hex << hash;

        return stream.str();
    }
}

uint32_t
Contract::getFunctionOffset(
    uint32_t hash
) {

    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        if (it->second.fnAddrHash == hash) {
            return it->first;
        }
    }

    return 0;
}

void
Contract::setData(
    bytes _data
) {
    m_vmState.setData(_data);
}

void
Contract::getFunction(
    uint32_t hash
) {
    if (m_byteCodeRuntime.empty()) return;

    m_vmState.clear();

    uint32_t offset = getFunctionOffset(hash);
    if (!offset) {
        printf("%s: Can't retrieve function offset.\n", __FUNCTION__);
        return;
    }

    string fullName = "";
    auto it = m_publicFunctions.find(hash);
    if (it != m_publicFunctions.end()) {
        fullName = it->second.abiName;
    }
    else {
        fullName = "func_" + porosity::to_hstring(offset);
    }
    printf("\nfunction %s {\n", fullName.c_str());

    m_vmState.m_eip = offset;
    m_vmState.executeByteCode(&m_byteCodeRuntime);

    printf("}\n\n");

    if (g_VerboseLevel > 1) m_vmState.displayStack();
}

void
Contract::forEachFunction(
    function<void(uint32_t)> const& _onFunction
) {
    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        if (it->second.fnAddrHash) _onFunction(it->second.fnAddrHash);
    }
}

void
Contract::printFunctions(
    void
)
{
    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        if (it->second.fnAddrHash) {
            auto func = m_publicFunctions.find(it->second.fnAddrHash);
            string name = "";
            if (func != m_publicFunctions.end()) name = func->second.name;
            printf("[+] Hash: 0x%08X (%s) (%d references)\n", it->second.fnAddrHash, name.c_str(), it->second.references.size());
            // getFunction(it->second.fnAddrHash);
        }
    }
}

uint32_t
Contract::getBlockSuccessorsCount(
    BasicBlockInfo *_block
) {
    return !!(_block->dstDefault) + !!(_block->dstJUMPI);
}

uint32_t
Contract::getBlockPredecessorsCount(
    BasicBlockInfo *_block
) {
    return _block->references.size();
}

BasicBlockInfo *
Contract::getBlockAt(
    uint32_t _offset
) {
    auto item = m_listbasicBlockInfo.find(_offset);
    if (item == m_listbasicBlockInfo.end()) return 0;

    BasicBlockInfo *t = &item->second;
    return t;
}

bool
Contract::StructureIfElse(
    BasicBlockInfo *_block
)
{
    if (getBlockSuccessorsCount(_block) != 2) return false;

    BasicBlockInfo *trueBlock = getBlockAt(_block->dstJUMPI);
    BasicBlockInfo *falseBlock = getBlockAt(_block->dstDefault);

    if ((getBlockSuccessorsCount(trueBlock) != 1) || (getBlockSuccessorsCount(falseBlock) != 1)) {
        return false;
    }

    if (trueBlock->dstDefault != falseBlock->dstDefault) return false;

    //_block->

    Statement ifStmt(StatementIf);
    //ifStmt.setCondition(_block->condAttr);
    ifStmt.NegateCondition();

    ifStmt.setBlocks(trueBlock, falseBlock);

    return true;
}

Statement
Contract::StructureIfs(
    BasicBlockInfo *_block
)
{
    Statement ifStmt(StatementIf);

    if (getBlockSuccessorsCount(_block) != 2) return ifStmt;

    BasicBlockInfo *trueBlock = getBlockAt(_block->dstJUMPI);
    BasicBlockInfo *falseBlock = getBlockAt(_block->dstDefault);

    if ((trueBlock && (getBlockSuccessorsCount(trueBlock) != 1)) || (falseBlock && (getBlockSuccessorsCount(falseBlock) != 1))) {
        return ifStmt;
    }

    uint32_t falseLocation = _block->dstDefault;

    if ((_block->nextDefault->instructions.size() == 2) && 
        (_block->nextDefault->instructions[1].offInfo.inst == Instruction::JUMP)) { // PUSH/JUMP
        falseLocation = _block->nextDefault->dstDefault;
        falseBlock = _block->nextDefault->nextDefault;
    }

    if (trueBlock && ((getBlockSuccessorsCount(trueBlock) == 1)) /*&&
        (trueBlock->dstDefault == falseLocation)*/) {
#ifdef _WIN32
        for each (auto instrState in _block->instructions) {
#else
        for (auto instrState : _block->instructions) {
#endif
            ifStmt.setCondition(instrState);
        }
        // ifStmt.NegateCondition();
        ifStmt.setBlocks(trueBlock, falseBlock);
        // ifStmt.print();
        ifStmt.setValid();
        // return ifStmt;
        return ifStmt;
    }

    return ifStmt;
}

bool
Contract::decompileBlock(
    SourceCode *decompiled_code,
    uint32_t _depth,
    BasicBlockInfo *_block
) {
    bool result = true;

    for (auto i = _block->instructions.begin();
        i != _block->instructions.end();
        ++i) {
        string name = "";
        uint32_t errCode = DCode_OK;

        if (i->stack.size()) name = InstructionContext::getDismangledRegisterName(&i->stack[0]);
        if (g_VerboseLevel > 5) {
            printf("0x%08x: %s [%s]\n", i->offInfo.offset, i->offInfo.instInfo.name.c_str(), name.c_str());
            displayStack(&i->stack);
            if(g_SingleStepping) getchar();
        }

        string exp;
        switch (i->offInfo.inst) {
        case Instruction::MLOAD:
            break;
        case Instruction::MSTORE:
        {
            auto next = i + 1;
            if ((i->stack.size() > 1) && (next->stack.size() > 0)) {
                if (i->stack[1].exp.size()) exp = next->stack[0].name + " = " + i->stack[1].exp + ";";
            }
            // displayStack(&i->stack);
            break;
        }
        case Instruction::CALL:
        {

            auto next = i + 1;
            int callType = int(i->stack[1].value);
            switch (callType) {
                case Sha256Type:
                case RipeMd160Type:
                    exp = next->stack[0].exp;
                break;
            }
        }
        case Instruction::SLOAD:
            // exp = "store[" + i->stack[0].name + "] ? " + i->stack[1].exp + ";";
            // displayStack(&i->stack);
            break;
        case Instruction::SSTORE:
        {

            string valueName = "";
            switch (i->stack[1].type) {
                case ConstantComputed:
                case Constant:
                {
                    stringstream mod;
                    mod << "0x" << std::hex << i->stack[1].value;
                    valueName = mod.str();
                    // mod << "0x" << std::hex << second->value;
                    break;
                }
                default:
                    valueName = (i->stack[1].exp.size()) ? i->stack[1].exp : i->stack[1].name;
                    break;
            }
            exp = "store[" + i->stack[0].name + "] = " + valueName + ";";
            if (_block->Flags & BlockFlags::NoMoreSSTORE) errCode |= DCode_Err_ReentrantVulnerablity;
            break;
        }
        case Instruction::RETURN:
            exp = "return " + i->stack[0].name + ";";
            result = false;
            break;
        case Instruction::STOP:
            exp = "return;";
            result = false;
            break;
        }
        if (exp.size()) {
            //DEPTH(_depth);
            //printf("%s\n", exp.c_str());
            decompiled_code->append(_depth, exp);
            if (errCode) decompiled_code->setErrCode(errCode);
        }
    }

    Statement s = StructureIfs(_block);
    if (s.isValid()) {
        //DEPTH(_depth);
        //printf("%s {\n", s.getStatementStr().c_str());
        decompiled_code->append(_depth, s.getStatementStr() + " {");
        decompileBlock(decompiled_code, _depth + 1, _block->nextJUMPI);
        //DEPTH(_depth); 
        //printf("}\n");
        decompiled_code->append(_depth, "}");
    }

    return result;
}

void
Contract::decompile(
    uint32_t _hash
) {
    uint32_t offset = getFunctionOffset(_hash);
    if (!offset) return;

    SourceCode decompiled_code;

    //printf("\nfunction %s {\n", getFunctionName(_hash).c_str());
    decompiled_code.append(0, "function " + getFunctionName(_hash) + " {");

    VMState newState = m_vmState;
    newState.m_basicBlocks = &m_listbasicBlockInfo;

    BasicBlockInfo *block = getBlockAt(offset);
    newState.executeFunction(block); // get stack status
    computeDominators();

    do {
        decompileBlock(&decompiled_code, 2, block);

        block = block->nextDefault;
    } while (block);

    decompiled_code.append(0, "}");
    // printf("}\n");

    decompiled_code.print();
    printf("LOC: %d\n", decompiled_code.loc());
}

bool
Contract::IsRuntimeCode(
    bytes code
)
/*++
    Description: 
        This function analyze the input bytecode to know if it is the runtime code.
        If not it returns the offset of the runtime bytecode in m_runtimeOffset
--*/
{
    bool hasCODECOPY = false;
    bool leaveFunction = false;

    dev::eth::eachInstruction(code, [&](uint32_t _offset, Instruction _instr, u256 const& _data) {
        // porosity::printInstruction(_offset, _instr, _data);

        if (leaveFunction) {
            if (hasCODECOPY && !m_runtimeOffset) {
                if (_instr == Instruction::STOP) return;
                m_runtimeOffset = _offset;
                if (g_VerboseLevel >= 5) printf("Runtime function found at offset = 0x%x\n", _offset);
            }
            return;
        }

        switch (_instr) {
            case Instruction::CODECOPY:
                hasCODECOPY = true;
                break;
            case Instruction::RETURN:
                leaveFunction = true;
                break;
        }
    });

    return !hasCODECOPY;
}
