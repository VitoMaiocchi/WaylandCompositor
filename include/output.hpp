#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <functional>
#include <list>
#include <memory>

namespace Layout {
    class Base;
}

namespace Output {

    extern wlr_output_layout* output_layout;
    extern wlr_scene* scene;

    void setup();
    void cleanup();

    class Buffer {
        public:
            Buffer();
            ~Buffer();
            void draw(std::function<void(cairo_t*, double scale)> draw, Extends ext);
        private:
            wlr_scene_buffer* scene_buffer;
    };

    //value must be between -1 and 1
    void adjustVolume(double vol);
    void muteVolume();
}