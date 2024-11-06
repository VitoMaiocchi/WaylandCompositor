#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <functional>
#include <list>

namespace Output {

    extern wlr_output_layout* output_layout;
    extern wlr_scene* scene;

    void setup();
    void cleanup();

    class Buffer {
        public:
            Buffer();
            ~Buffer();
            void draw(std::function<void(cairo_t*)> draw, Extends ext);
        private:
            wlr_scene_buffer* scene_buffer;
    };

    class Monitor {
        public:
            Monitor(wlr_output* output);
            Extends getExtends(bool full);
            void frameNotify();
            void updateState(const wlr_output_state* state);
        private:
            Extends extends;
            wlr_output* output;
    };

    extern std::list<Monitor*> monitors;
}