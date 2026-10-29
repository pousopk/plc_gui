#pragma once
#include "plc/plc_value.hpp"
#include <lmlib-profinet/ValueCache.hpp>

// The latest known value of every polled tag, keyed by TagConfig::id.
// Written by PlcIoThread, read by the UI and LogManager. Lives in
// lmlib-profinet; these aliases keep it usable unqualified across the app.
using profinet::CachedValue;
using profinet::ValueCache;
using profinet::ValueMap;
