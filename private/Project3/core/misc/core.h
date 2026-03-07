#pragma once

#include <iostream>
#include <string>

class core_t
{
public:
    #define console_log( fmt, ... ) console_log( " [>] " fmt "", ##__VA_ARGS__ )

}; inline core_t core;