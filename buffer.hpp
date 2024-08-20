#include "includes.hpp"
#include <cairo/cairo.h>
#include <functional>

class Buffer {
    public:
    Buffer(wlr_scene_tree* parent, wlr_box extends);
    void setPosition(int x, int y);
    void draw(std::function<void(cairo_t*)> draw);

    private:
    wlr_box extends;
    wlr_scene_buffer* scene_buffer;
};