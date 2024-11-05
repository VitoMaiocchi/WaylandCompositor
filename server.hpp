#pragma once

#include "includes.hpp"

namespace Server {

    extern wl_display* display;
	extern wlr_backend* backend;
    extern wlr_renderer* renderer;
	extern wlr_allocator* allocator;

    bool setup();
}