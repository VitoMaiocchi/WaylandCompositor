#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <functional>
#include <list>
#include <memory>

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

    //logical representation of one virtual Display (can be assigned to multiple monitors)
    class Display {
        public: 
            Display(Extends ext);
        private:
            Extends extends;
    };

    typedef std::weak_ptr<Display> DisplayPtr;

    extern std::list<DisplayPtr> displays;
}