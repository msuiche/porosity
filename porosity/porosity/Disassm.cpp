/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Disassm.cpp

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/

#include "Porosity.h"

namespace porosity {

    string
    getInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data,
        bool noSpace
    ) {
        stringstream stream;

        stream << "0x"
            << std::setfill('0') << std::setw(sizeof(_offset) * 2)
            << std::hex << _offset;

        stream << " ";
        // printf("0x%08X: ", _offset);

        InstructionInfo info = dev::eth::instructionInfo(_instr);

        if (!dev::eth::isValidInstruction(_instr)) {
            stream << std::setfill('0') << std::setw(sizeof(char) * 2)
                << std::hex << int(_instr);

            stream << "                    !INVALID!";
            // printf("%02X                    !INVALID!", int(_instr));
        }
        else
        {
            u256 data = _data;

            // Data
            // printf("%02X ", int(_instr));
            stream << std::setfill('0') << std::setw(sizeof(char) * 2)
                << std::hex << int(_instr);
            stream << " ";

            for (int i = 0; i < 5; ++i) {
                if (i < info.additional) {
                    if (i == 4) {
                        // printf("+  ");
                        stream << "+  ";
                    }
                    else {
                        uint8_t dataByte = int(data & 0xFF);
                        data >>= 8;
                        //printf("%02X ", dataByte);
                        stream << std::setfill('0') << std::setw(sizeof(char) * 2)
                            << std::hex << int(dataByte) << "  ";
                    }
                }
                else {
                    // printf("   ");
                    if (!noSpace) stream << "    ";
                }
            }

            //printf("    ");
            if (!noSpace) stream << "    ";
            else stream << " | ";

            // Disassembly
            // InstructionInfo info = dev::eth::instructionInfo(_instr);
            // u256 data = _data;
            data = _data;
            //printf("%s ", info.name.c_str());
            stream << info.name << " ";
            for (int i = 0; i < info.additional; ++i) {
                uint8_t dataByte = int(data & 0xFF);
                data >>= 8;
                // printf("0x%02X ", dataByte);
                stream << std::setfill('0') << std::setw(sizeof(char) * 2)
                    << std::hex << int(dataByte) << " ";
            }

    #if 0 // TODO: Find out how to call resolveBranchName
            // Resolve branch name.
            if (_instr == Instruction::PUSH1) {
                string name = resolveBranchName(int(_data & 0xFF));
                if (name.size()) printf(" ; %s", name.c_str());
            }
    #endif
        }
        // printf("\n");

        return stream.str();
    }

    void
    printInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data
    ) {
        printf("%s\n", porosity::getInstruction(_offset, _instr, _data, false).c_str());
    }

    string
    buildNode(
        bytes const& _mem,
        uint32_t offset
    ) {
        string graphNode = "";

        // Collect all the instruction from a given basic block.
        dev::eth::eachInstruction(_mem, [&](uint32_t _offset, Instruction _instr, u256 const& _data) {
            graphNode += porosity::getInstruction(offset + _offset, _instr, _data, false).c_str();
            graphNode += "\\l";
        });

        return graphNode;
    }

}