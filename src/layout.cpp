#define LOGGER_CATEGORY Logger::LAYOUT
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include <list>
#include <algorithm>
#include <cassert>

#define TitleBarHeight 30

namespace Layout {

/*
Surface::Toplevel* focused_toplevel;

void inline setFocus(Surface::Toplevel* surface) {
    if(!surface) return;
    if(focused_toplevel) {
        if(focused_toplevel == surface) return;
        focused_toplevel->setFocus(false);
        focused_toplevel = surface;
    }
    focused_toplevel = surface;
    surface->setFocus(true);
}
*/

//TODO: das chammer abstract mache für meh tiling layouts
class TilingLayout {
    std::list<Surface::Toplevel*> surfaces;

    public:
    void addSurface(Surface::Toplevel* surface) {
        surfaces.push_back(surface);
    }

    Surface::Toplevel* removeSurface(Surface::Toplevel* surface) {
        auto it = std::find(surfaces.begin(), surfaces.end(), surface);
        assert(it != surfaces.end());

        it = surfaces.erase(it);

        if(surfaces.begin() == surfaces.end()) return nullptr;
        if(it != surfaces.end()) return *it;
        else return *surfaces.begin();
    }

    void setVisible(bool visibility) {
        for(auto surface : surfaces) surface->setVisibility(visibility);
    }

    Surface::Toplevel* getSurfaceAtLocation(const double x, const double y, Surface::Toplevel* include_children) {
        if(include_children) {
            auto it = std::find(surfaces.begin(), surfaces.end(), include_children);
            assert(it != surfaces.end());
            if((*it)->contains(x,y,true)) return include_children; 
        }

        //it checks include_children twice but I can live with that
        for(auto s : surfaces) if(s->contains(x,y, false)) return s;
        return nullptr;
    }

    void updateLayout(Extends &extends) {
        const uint n = surfaces.size();
        debug("Update layout: n={}; Extends={}", n, extends);
        if(n == 0) return;
        const uint h = extends.width / n;
        auto i = surfaces.begin();
        Extends ext = extends;
        ext.width = extends.width - (n-1)*h;
        debug("Update layout surface: Extends={}", ext);
        (*i)->setExtends(ext);
        (*i)->setAvailableArea(extends); //TODO: suboptimal das musi zu desktop schiebe
        ext.x += ext.width;
        ext.width = h;
        i++;
        while(i != surfaces.end()) {
            debug("Update layout surface: Extends={}", ext);
            (*i)->setExtends(ext);
            (*i)->setAvailableArea(extends);
            ext.x += h;
            i++;
        }
    };

    typedef std::list<Surface::Toplevel*>::iterator iterator;

    iterator begin() {
        return surfaces.begin();
    }

    iterator end() {
        return surfaces.end();
    }
};

class Desktop {
    TilingLayout tilingLayout;
    Extends extends;
    Surface::Toplevel* focused_toplevel = nullptr;
    bool focused = false;

    public:
    void setFocusedSurface(Surface::Toplevel* surface) {
        if(!surface || focused_toplevel == surface) return;
        if(!focused) {
            focused_toplevel = surface;
            return;
        }
        if(focused_toplevel) focused_toplevel->setFocus(false);
        focused_toplevel = surface;
        focused_toplevel->setFocus(true);
    }

    Surface::Toplevel* getFocusedSurface() {
        return focused_toplevel;
    }

    void addSurface(Surface::Toplevel* surface) {
        tilingLayout.addSurface(surface);
        tilingLayout.updateLayout(extends);
        setFocusedSurface(surface);
    }

    void removeSurface(Surface::Toplevel* surface) {
        auto new_focus = tilingLayout.removeSurface(surface);
        tilingLayout.updateLayout(extends);
        if(focused_toplevel != surface) return;
        if(new_focus) setFocusedSurface(new_focus);
        else focused_toplevel = nullptr;
    }

    void setFocus(bool focus) {
        //TODO: nöd automatisch focus remove wenn mer uf en andere bildschirm gaht (de surface focus semi global mache)
        focused = focus;
        if(!focused_toplevel) {
            auto it = tilingLayout.begin();
            if(it == tilingLayout.end()) {
                Input::setCursorFocus(nullptr);
                return;
            }
            focused_toplevel = *it;
        }

        focused_toplevel->setFocus(focus);
        Input::setCursorFocus(focused_toplevel);
    }

    void setVisibility(bool visibility) {
        tilingLayout.setVisible(visibility);
    }

    void updateExtends(Extends ext) {
        debug("Desktop update extends: Extends={}", extends);
        extends = ext;
        tilingLayout.updateLayout(extends);
    }

    Surface::Toplevel* getSurfaceAtLocation(const double x, const double y, bool include_children) {
        if(include_children) return tilingLayout.getSurfaceAtLocation(x,y, focused_toplevel);
        return tilingLayout.getSurfaceAtLocation(x,y, nullptr);
    }
};


class Display {
    Extends extends;
    Titlebar titlebar;
    Desktop desktops[9];
    uint current_desktop = 0;
    bool focus = false;

    public:
    Display(Extends ext) : extends(ext), titlebar(ext.withHeight(TitleBarHeight)) {
        Extends layout_ext = ext;
        layout_ext.height -= 30;
        layout_ext.y += 30;
        for(auto &desktop : desktops) desktop.updateExtends(layout_ext);
    }

    void updateExtends(Extends ext) {
        extends = ext;
        Extends layout_ext = ext;
        layout_ext.height -= 30;
        layout_ext.y += 30;
        for(auto &desktop : desktops) desktop.updateExtends(layout_ext);
        titlebar.updateExtends(ext.withHeight(TitleBarHeight));
    }

    bool contains(const double x, const double y) {
        return extends.contains(x,y);
    }

    void setFocus(bool focus) {
        this->focus = focus;
        desktops[current_desktop].setFocus(focus);
    }

    void handleCursorMovement(const double x, const double y) {
        const auto surface = desktops[current_desktop].getSurfaceAtLocation(x,y, true);
        desktops[current_desktop].setFocusedSurface(surface);
        Input::setCursorFocus(surface);
    }

    void addSurface(Surface::Toplevel* surface) {
        desktops[current_desktop].addSurface(surface);
    }

    void removeSurface(Surface::Toplevel* surface) {
        desktops[current_desktop].removeSurface(surface);
    }

    void setDesktop(uint desktop) {
        titlebar.updateDesktop(desktop);
        desktops[current_desktop].setVisibility(false);
        desktops[current_desktop].setFocus(false);
        current_desktop = desktop;
        desktops[current_desktop].setVisibility(true);
        desktops[current_desktop].setFocus(true);
    }

    Surface::Toplevel* getFocusedSurface() {
        return desktops[current_desktop].getFocusedSurface();
    }
};

std::list<Display*> displays;
Display* focused_display = nullptr;

inline void setFocusedDisplay(Display* display) {
    if(!display || focused_display == display) return;
    if(focused_display) focused_display->setFocus(false);
    focused_display = display;
    focused_display->setFocus(true);
}

inline Display* getFocusedDisplay() {
    assert(displays.size() > 0);
    if(!focused_display) setFocusedDisplay(*displays.begin());
    return focused_display;
}

void addSurface(Surface::Toplevel* surface) {
    getFocusedDisplay()->addSurface(surface);
    surface->setVisibility(true);
}

void removeSurface(Surface::Toplevel* surface) {
    getFocusedDisplay()->removeSurface(surface);
    surface->setVisibility(false);
}

void handleCursorMovement(const double x, const double y) {
    if(!getFocusedDisplay()->contains(x,y))
        for(Display* display : displays) if(display->contains(x,y)) setFocusedDisplay(display);
    
    getFocusedDisplay()->handleCursorMovement(x,y);
}

void setDesktop(uint desktop) {
    getFocusedDisplay()->setDesktop(desktop);
}

void killClient() {
    warn("KILL CLIENT");
    const auto display = getFocusedDisplay();
    if(!display) return;
    const auto s = display->getFocusedSurface();
    if(s) s->kill();
}

}

Output::Display::Display(Extends ext) {
    displayLayout = new Layout::Display(ext);
    Layout::displays.push_back(displayLayout);
}

Output::Display::~Display() {
    Layout::displays.remove(displayLayout);
    if(Layout::focused_display == displayLayout) Layout::focused_display = nullptr;
    delete displayLayout;
}

void Output::Display::updateExtends(Extends ext) {
    displayLayout->updateExtends(ext);
}