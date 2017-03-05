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

void
Contract::getBasicBlocks(
    void
)
{
    if (m_byteCodeRuntime.empty()) return;

    BasicBlockInfo emptyBlockInfo = { 0 };

    m_instructions.clear();
    m_listbasicBlockInfo.clear();

    // Exitnode
    m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(NODE_DEADEND, emptyBlockInfo));

    // Entrypoint
    m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(0, emptyBlockInfo));

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
            // printf("JUMPDEST: 0x%08X\n", _offset);
            m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(_offset, emptyBlockInfo));
        }
    });

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
                        auto newBlock = m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(next->offset, emptyBlockInfo));
                        newBlock->second.size = nextInstrBlockSize;
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

    return;
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
        auto block = m_listbasicBlockInfo.find(next);
        next = block->second.dstDefault;

        if (block->second.dstJUMPI) {
            walkAndConnectNodes(_hash, block->second.dstJUMPI);
        }

        if (!next) break;
        if (next == NODE_DEADEND) {
            auto exitNode = m_exitNodesByHash.find(_hash);
            block->second.dstDefault = exitNode->second;
            break; // next
        }
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
       if (_conditional == NodeType::ConditionalNode) it->second.dstJUMPI = _block;
       else it->second.dstDefault = _block;
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
        printf("}");
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

    m_abi_json = nlohmann::json::parse(abi.c_str());

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