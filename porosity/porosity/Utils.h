/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Utils.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _UTILS_H_
#define _UTILS_H_

#include "Porosity.h"

namespace porosity {

    u256 exp256(u256 _base, u256 _exponent);

    std::string to_hstring(uint32_t i);
};

#endif