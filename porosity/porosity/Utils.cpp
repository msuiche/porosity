/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Utils.cpp

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#include "Porosity.h"

namespace porosity {

    u256 exp256(u256 _base, u256 _exponent)
    {
        using boost::multiprecision::limb_type;
        u256 result = 1;
        while (_exponent)
        {
            if (static_cast<limb_type>(_exponent) & 1)	// If exponent is odd.
                result *= _base;
            _base *= _base;
            _exponent >>= 1;
        }
        return result;
    }

    string to_hstring(uint32_t i)
    {
        stringstream stream;
        stream << "0x"
            << std::setfill('0') << std::setw(sizeof(i) * 2)
            << std::hex << i;
        return stream.str();
    }
}