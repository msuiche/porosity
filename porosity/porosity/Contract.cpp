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

    BasicBlockInfo emptyBlockInfo = { 0,{ 0, RegularNode }, 0, "" };

    m_instructions.clear();
    m_listbasicBlockInfo.clear();

    m_listbasicBlockInfo.insert(m_listbasicBlockInfo.begin(), pair<uint32_t, BasicBlockInfo>(NODE_DEADEND, emptyBlockInfo));

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
        // basicBlockOffset offCurrent->offset;

        switch (currentOffInfo->inst) {
            case Instruction::JUMPDEST:
            {
                uint32_t prevBasicBlockOffset = basicBlockOffset;
                basicBlockOffset = currentOffset;

                auto it = m_listbasicBlockInfo.find(currentOffset);
                bool isPublicFunction = ((it != m_listbasicBlockInfo.end()) && (it->second.fnAddrHash));

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
                        addBlockReference((uint32_t)basicBlockOffset, prevBasicBlockOffset, false, RegularNode);
                    }
                }
                break;
            }
            case Instruction::STOP:
            {
                tagBasicBlock(basicBlockOffset, "STOP");
                break;
            }
            case Instruction::RETURN:
            {
                tagBasicBlock(basicBlockOffset, "RETURN");
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

                        addBlockReference((uint32_t)jmpDest, basicBlockOffset, fnAddrHash, ConditionalNode);
                    }
                    if (VERBOSE_LEVEL >= 1) printf("%s: function @ 0x%08X (hash = 0x%08x)\n",
                        __FUNCTION__, basicBlockOffset, fnAddrHash);
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
                        addBlockReference((uint32_t)jmpDest, basicBlockOffset, false, RegularNode);
                    }
                    else {
                        addBlockReference(int(NODE_DEADEND), basicBlockOffset, false, ExitNode);
                    }
                }
                if (VERBOSE_LEVEL >= 1) printf("%s: branch @ 0x%08X\n", __FUNCTION__, basicBlockOffset);
                break;
            }
        }
    }
    return;
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
Contract::addBlockReference(
    uint32_t _dest,
    uint32_t _src,
    uint32_t _fnAddrHash,
    NodeType _conditional
)
{
   auto it = m_listbasicBlockInfo.find(_dest);
   if (it != m_listbasicBlockInfo.end()) {
       uint32_t numrefs = it->second.numrefs;
       Xref ref;
       ref.offset = _src;
       ref.conditional = _conditional;
       it->second.references[numrefs] = ref;
       it->second.numrefs++;
       it->second.fnAddrHash = _fnAddrHash;
       return true;
   }

   return false;
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

void
Contract::printBlockReferences(
    void
)
{
    // DEBUG
    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        printf("(dest = 0x%08X, numrefs = 0x%08X, refs = {", it->first, it->second.numrefs);
        for (uint32_t i = 0; i < it->second.numrefs; i++) {
            printf("0x%08x", it->second.references[i].offset);
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
    void
)
{
    // DEBUG
#if 1
    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        printf("(dest = 0x%08X, numrefs = 0x%08X, refs = {", it->first, it->second.numrefs);
        for (uint32_t i = 0; i < it->second.numrefs; i++) {
            printf("0x%08x ", it->second.references[i].offset);
        }
        printf("}");
        if (it->second.fnAddrHash) {
            printf(", hash = 0x%08X, ", it->second.fnAddrHash);
            printf("str = %s, ", getFunctionName(it->second.fnAddrHash).c_str());
        }
        printf(")");
        printf("\n");
    }
#endif

    string graph;

    graph = "digraph porosity {\n";
    graph += "rankdir = TB;\n";
    graph += "size = \"12\"\n";
    graph += "node[shape = square];\n";

    for (auto it = m_listbasicBlockInfo.begin(); it != m_listbasicBlockInfo.end(); ++it) {
        if (it->second.numrefs) {
            for (uint32_t i = 0; i < it->second.numrefs; i++) {
                uint32_t offset_str = it->second.references[i].offset;
                NodeType nodeType = it->second.references[i].conditional;
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
        }
        else {
            graph += "    ";
            graph += "\"" + porosity::to_hstring(int(NODE_DEADEND)) + "\"" + " -> " + "\"" + porosity::to_hstring(it->first) + "\"" + "[label = \""  + +it->second.name.c_str() + "\"";
            graph += " color=black fillcolor = lightgray";
            graph += "]; ";
            graph += "\n";
        }
    }

    graph += "}\n";
    return graph;
}

void
Contract::setABI(
    string abi
) {
    m_abi_json = nlohmann::json::parse(abi);

    m_publicFunctions.clear();

    string dmp = m_abi_json.dump();

    int maxMethods = m_abi_json.size();
    for (int i = 0; i < maxMethods; i += 1) {
        stringstream name;
        name.clear();
        name << m_abi_json[i]["name"];
        name << "(";
        int maxInputs = m_abi_json[i]["inputs"].size();
        for (int j = 0; j < maxInputs; j += 1) {
            name << m_abi_json[i]["inputs"][j]["type"];
            if ((j + 1) < maxInputs) name << ",";
        }
        name << ")";

        string abiMethod = name.str();
        abiMethod.erase(std::remove(abiMethod.begin(), abiMethod.end(), '"'), abiMethod.end());
        if (VERBOSE_LEVEL >= 5) printf("%s: Name: %s\n", __FUNCTION__, abiMethod.c_str());
        uint32_t hashMethod;

        dev::FixedHash<4> hash(dev::keccak256(abiMethod));
#ifdef _WIN32
        sscanf_s(hash.hex().c_str(), "%x", &hashMethod);
#else
        sscanf(hash.hex().c_str(), "%x", &hashMethod);
#endif
        FunctionDef def;
        if (m_abi_json[i]["type"] == "function") {
            string abiMethodName = m_abi_json[i]["name"];
            abiMethodName.erase(std::remove(abiMethodName.begin(), abiMethodName.end(), '"'), abiMethodName.end());
            def.name = abiMethodName;
            def.abiName = abiMethod;
            m_publicFunctions.insert(m_publicFunctions.begin(), pair<uint32_t, FunctionDef>(hashMethod, def));
        }
        if (VERBOSE_LEVEL >= 5) printf("%s: signature: 0x%08x\n", __FUNCTION__, hashMethod);
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

    auto it = m_publicFunctions.find(hash);
    uint32_t offset = getFunctionOffset(hash);

    if ((it == m_publicFunctions.end()) || (!offset)) {
        printf("ERROR: Hash not found.\n");
        return;
    }

    string fullName = it->second.abiName;
    printf("\nfunction %s {\n", fullName.c_str());

    m_vmState.m_eip = offset;
    m_vmState.executeByteCode(&m_byteCodeRuntime);

    printf("}\n\n");

    m_vmState.displayStack();
}