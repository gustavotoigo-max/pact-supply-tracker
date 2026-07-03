#pragma once

#include "Nexus.h"

namespace addon
{
    inline constexpr const char* Name = "Pact Supply Tracker";
    inline constexpr const char* LogChannel = "PactSupply";

    extern AddonAPI_t* Api;

    void Log(ELogLevel level, const char* message);
}
