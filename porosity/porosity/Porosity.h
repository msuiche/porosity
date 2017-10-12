/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Porosity.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#include <stdlib.h>
#include <stdint.h>
#include <sstream>
#include <map>
#include <vector>
#include <boost/dynamic_bitset.hpp>

#include "Common.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/version.hpp>

#include "json/json.hpp"

#include "CommonData.h"
#include "Assert.h"
#include "Output.h"

using namespace std;
namespace pt = boost::property_tree;

#include "SHA3.h"
#include "PorosityDefs.h"
#include "Disassm.h"
#include "VMState.h"
#include "Statement.h"
#include "Contract.h"
#include "Utils.h"
#include "Debug.h"

#define VERBOSE_LEVEL 0

extern uint32_t g_VerboseLevel; // VERBOSE_LEVEL
extern bool g_SingleStepping;

using namespace dev;
using namespace dev::eth;