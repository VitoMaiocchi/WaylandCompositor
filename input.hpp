#pragma once

#include "includes.hpp"

namespace Input {
    extern wlr_seat* seat;

    void setup();
    void cleanup();
}