/*++
Copyright (c) 2017, Matthieu Suiche

Module Name:
    Disassm.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _DISASSM_H_
#define _DISASSM_H_

#include "Instruction.h"

namespace porosity {

    string
    getInstruction(
        uint32_t _offset,
        dev::eth::Instruction _instr,
        dev::u256 const& _data
    );

    void
    printInstruction(
        uint32_t _offset,
        dev::eth::Instruction _instr,
        dev::u256 const& _data
    );

    string
    buildNode(
        dev::bytes const& _mem,
        uint32_t offset
    );
}
#endif