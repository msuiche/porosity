#ifndef _DISASSM_H_
#define _DISASSM_H_

#include "Instruction.h"

namespace porosity {

    void
        printInstruction(
            uint32_t _offset,
            dev::eth::Instruction _instr,
            dev::u256 const& _data
        );

}
#endif