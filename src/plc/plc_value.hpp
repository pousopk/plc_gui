#pragma once
#include <lmlib-profinet/PlcValue.hpp>

// PLC value types and their S7 byte encoding live in lmlib-profinet; these
// aliases keep them usable unqualified across the app.
using profinet::PlcType;
using profinet::PlcValue;
using profinet::decodePlcValue;
using profinet::encodePlcValue;
using profinet::plcTypeByteSize;
using profinet::plcValueAsDouble;
