#include <stdlib.h>
#include <stdint.h>
#include <sstream>
#include <map>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/version.hpp>

#include "json/json.hpp"

#include "Common.h"
#include "Assert.h"

#include "Utils.h"

#include "SHA3.h"
#include "Instruction.h"
#include "Disassm.h"
#include "VMState.h"
#include "Contract.h"

#define VERBOSE_LEVEL 1

using namespace std;
using namespace dev;
using namespace dev::eth;