#include "layout.hpp"
#include "input.hpp"
#include <list>
#include <algorithm>
#include <cassert>

namespace Layout {

    wlr_box screen_ext = {0,0,0,0};
    std::list<Surface::Toplevel*> surfaces;
    Surface::Toplevel* focused_toplevel;

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

        debug("UPDATE LAYOUT end");
    }

    void setScreenExtends(wlr_box extends) {
        extends.y += 30;
        extends.height -= 30;
        screen_ext = extends;
        updateLayout();
    }

    void inline setFocus(Surface::Toplevel* surface) {
        debug("SET FOCUS");
        if(!surface) return;
        if(focused_toplevel) {
            if(focused_toplevel == surface) return;
            focused_toplevel->setFocus(false);
            focused_toplevel = surface;
        }
        focused_toplevel = surface;
        surface->setFocus(true);
        debug("SET FOCUS end");
    }

    void addSurface(Surface::Toplevel* surface) {
        debug("ADD SURFACE");
        surfaces.push_back(surface);
        updateLayout();
        setFocus(surface);
        debug("ADD SURFACE end");
    }

    void removeSurface(Surface::Toplevel* surface) {
        auto it = std::find(surfaces.begin(), surfaces.end(), surface);
        assert(it != surfaces.end());

        it = surfaces.erase(it);
        updateLayout();

        if(surface == focused_toplevel) {
            if(surfaces.begin() == surfaces.end()) {
                focused_toplevel = nullptr;
                return;
            }

            if(it != surfaces.end()) focused_toplevel = *it;
            else focused_toplevel = *surfaces.begin();
            
            focused_toplevel->setFocus(true);
        }
    }

    Surface::Toplevel* getSurfaceAtPosition(const int x, const int y) {
        for(auto i = surfaces.begin(); i != surfaces.end(); i++) if((*i)->contains(x,y)) return *i;
        return NULL;
    }

    void handleCursorMovement(const double x, const double y) {
        auto surface = getSurfaceAtPosition(x,y);
        setFocus(surface);
        Input::setCursorFocus(surface);
    }
}