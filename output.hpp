#pragma once

#include "includes.hpp"

namespace Output {

    extern wlr_output_layout* output_layout;
    extern wlr_scene* scene;

    void setup();
    void cleanup();
}