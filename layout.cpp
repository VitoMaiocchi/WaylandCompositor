#include "layout.hpp"

namespace Layout {

    //TEMPORARY LAYOUT MANAGER mit nur eine surface
    wlr_box screen_ext = {0,0,0,0};
    ToplevelSurface* s;

    void setScreenExtends(wlr_box extends) {
        screen_ext = extends;
        if(s) s->setExtends(screen_ext);
    }

    void addSurface(ToplevelSurface* surface) {
        s = surface;
        s->setExtends(screen_ext);
    }

    ToplevelSurface* removeSurface(ToplevelSurface* surface) {
        s = NULL;
        return NULL;
    }

    ToplevelSurface* getSurfaceAtPosition(const int x, const int y) {
        return s;
    }
}