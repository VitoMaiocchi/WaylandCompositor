#pragma once
#include "includes.hpp"
#include "buffer.hpp"

class Titlebar {

    public:
    Titlebar(wlr_box ext);
    void updateExtends(wlr_box ext);
    void draw(cairo_t* cr);

    private:
    wlr_box extends;
    Buffer buffer;

};