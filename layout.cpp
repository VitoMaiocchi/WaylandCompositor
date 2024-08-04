#include "layout.hpp"
#include <list>
#include <algorithm>
#include <cassert>

namespace Layout {

    wlr_box screen_ext = {0,0,0,0};
    std::list<ToplevelSurface*> surfaces;

    void updateLayout() {
        debug("UPDATE LAYOUT");
        const uint n = surfaces.size();
        if(n == 0) return;
        const uint h = screen_ext.width / n;
        auto i = surfaces.begin();
        wlr_box ext = screen_ext;
        ext.width = screen_ext.width - (n-1)*h;
        (*i)->setExtends(ext);
        ext.x += screen_ext.width - (n-1)*h;
        i++;
        while(i != surfaces.end()) {
            (*i)->setExtends(ext);
            ext.x += h;
            i++;
        }

        debug("AHHH");
    }

    void setScreenExtends(wlr_box extends) {
        screen_ext = extends;
        updateLayout();
    }

    void addSurface(ToplevelSurface* surface) {
        debug("ADD SURFACE");
        surfaces.push_back(surface);
        updateLayout();
    }

    ToplevelSurface* removeSurface(ToplevelSurface* surface) {
        auto it = std::find(surfaces.begin(), surfaces.end(), surface);
        assert(it != surfaces.end());

        it = surfaces.erase(it);
        updateLayout();
        if(it != surfaces.end()) return *it;
        if(surfaces.begin() == surfaces.end()) return NULL;
        return *surfaces.begin();
    }

    ToplevelSurface* getSurfaceAtPosition(const int x, const int y) {
        for(auto i = surfaces.begin(); i != surfaces.end(); i++) if((*i)->contains(x,y)) return *i;
        return NULL;
    }
}