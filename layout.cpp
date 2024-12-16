#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include <list>
#include <algorithm>
#include <cassert>

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

    Surface::Toplevel* getSurfaceAtLocation(const double x, const double y) {
        for(auto s : surfaces) if(s->contains(x,y)) return s;
        return nullptr;
    }

    void updateLayout(Extends &extends) {
        const uint n = surfaces.size();
        if(n == 0) return;
        const uint h = extends.width / n;
        auto i = surfaces.begin();
        wlr_box ext = extends;
        ext.width = extends.width - (n-1)*h;
        (*i)->setExtends(ext);
        ext.x += extends.width - (n-1)*h;
        i++;
        while(i != surfaces.end()) {
            (*i)->setExtends(ext);
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
            if(it == tilingLayout.end()) return;
            focused_toplevel = *it;
        }

        focused_toplevel->setFocus(focus);
    }

    void setVisibility(bool visibility) {
        tilingLayout.setVisible(visibility);
    }

    void updateExtends(Extends ext) {
        extends = ext;
        tilingLayout.updateLayout(ext);
    }

    Surface::Toplevel* getSurfaceAtLocation(const double x, const double y) {
        return tilingLayout.getSurfaceAtLocation(x,y);
    }
};

//FIXME: temorary workaround. Ich burch e besseri extends class
Extends t_height(Extends ext) {
    ext.height = 30;
    return ext;
}

//TODO: dem en name ge wo nöd ass isch (Display und s andere display to output move)
class Display {
    Extends extends;
    Titlebar titlebar;
    Desktop desktop;

    public:
    Display(Extends ext) : extends(ext), titlebar(t_height(ext)) {
        Extends layout_ext = ext;
        layout_ext.height -= 30;
        layout_ext.y += 30;
        desktop.updateExtends(layout_ext);
    }

    void updateExtends(Extends ext) {
        extends = ext;
        Extends layout_ext = ext;
        layout_ext.height -= 30;
        layout_ext.y += 30;
        desktop.updateExtends(layout_ext);
        titlebar.updateExtends(t_height(ext));
    }

    //FIXME: temorary workaround. Ich burch e besseri extends class
    bool contains(const double x, const double y) {
        if(extends.x > x) return false;
        if(extends.x + extends.width < x) return false;
        if(extends.y > y) return false;
        if(extends.y + extends.height < y) return false;
        return true;
    }

    void setFocus(bool focus) {
        desktop.setFocus(focus);
    }

    void handleCursorMovement(const double x, const double y) {
        const auto surface = desktop.getSurfaceAtLocation(x,y);
        desktop.setFocusedSurface(surface);
        Input::setCursorFocus(surface);
    }

    void addSurface(Surface::Toplevel* surface) {
        desktop.addSurface(surface);
    }

    void removeSurface(Surface::Toplevel* surface) {
        desktop.removeSurface(surface);
    }

    void setDesktop(uint desktop) {
        titlebar.updateDesktop(desktop);
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