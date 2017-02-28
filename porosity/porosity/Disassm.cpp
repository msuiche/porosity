#include "Porosity.h"

namespace porosity {

    void
    printInstruction(
        uint32_t _offset,
        Instruction _instr,
        u256 const& _data
    ) {
        printf("0x%08X: ", _offset);

        InstructionInfo info = dev::eth::instructionInfo(_instr);

        if (!dev::eth::isValidInstruction(_instr)) {
            printf("%02X                    !INVALID!", int(_instr));
        }
        else
        {
            u256 data = _data;

            // Data
            printf("%02X ", int(_instr));

            for (int i = 0; i < 5; ++i) {
                if (i < info.additional) {
                    if (i == 4) {
                        printf("+  ");
                    }
                    else {
                        uint8_t dataByte = int(data & 0xFF);
                        data >>= 8;
                        printf("%02X ", dataByte);
                    }
                }
                else {
                    printf("   ");
                }
            }

            printf("    ");

            // Disassembly
            // InstructionInfo info = dev::eth::instructionInfo(_instr);
            // u256 data = _data;
            data = _data;
            printf("%s ", info.name.c_str());
            for (int i = 0; i < info.additional; ++i) {
                uint8_t dataByte = int(data & 0xFF);
                data >>= 8;
                printf("0x%02X ", dataByte);
            }

    #if 0 // TODO: Find out how to call resolveBranchName
            // Resolve branch name.
            if (_instr == Instruction::PUSH1) {
                string name = resolveBranchName(int(_data & 0xFF));
                if (name.size()) printf(" ; %s", name.c_str());
            }
    #endif
        }
        printf("\n");
    }

}