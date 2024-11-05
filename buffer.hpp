#pragma once

#include "includes.hpp"
#include <cairo/cairo.h>
#include <functional>

class Buffer {
    public:
    Buffer(wlr_scene_tree* parent);
    void setPosition(int x, int y);
    void draw(std::function<void(cairo_t*)> draw, uint width, uint height);

    private:
    wlr_scene_buffer* scene_buffer;
};