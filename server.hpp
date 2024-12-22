#pragma once

#include "includes.hpp"

#include <functional>

namespace Server {

    extern wl_display* display;
	extern wlr_backend* backend;
    extern wlr_renderer* renderer;
	extern wlr_allocator* allocator;
    extern wlr_compositor* compositor;

    typedef std::function<void(void* data)> EventCallback;

    bool setup();
    void queueEvent(EventCallback callback, void* data);
}