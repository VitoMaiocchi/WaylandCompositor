#define LOGGER_CATEGORY Logger::LAYOUT
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include <list>
#include <algorithm>
#include <cassert>

#define TitleBarHeight 30

namespace Layout {

template <typename IteratorType>
class TilingLayout {
    public:
    typedef IteratorType Iterator;

    virtual void addSurface(Surface::Toplevel* surface) = 0;
    virtual Surface::Toplevel* removeSurface(Surface::Toplevel* surface) = 0;
    virtual Surface::Toplevel* getSurfaceAtLocation(const double x, const double y, Surface::Toplevel* include_children) = 0;
    virtual void updateLayout(Extends &extends, std::function<void(Surface::Toplevel*, Extends)> set_extends) = 0;
    void updateLayout(Extends &extends) {
        updateLayout(extends, [](Surface::Toplevel* toplevel, Extends ext){
            toplevel->setExtends(ext);
        });
    }
    virtual Iterator begin() = 0;
    virtual Iterator end() = 0;

    void setVisible(bool visibility) {
        for(auto it = begin(); it != end(); it++) {
            (*it)->setVisibility(visibility);
        }
    }
};

/*
class LinearTilingLayout : public TilingLayout<std::list<Surface::Toplevel*>::iterator> {
    protected:
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

    Iterator begin() {
        return surfaces.begin();
    }

    Iterator end() {
        return surfaces.end();
    }

    virtual void updateLayout(Extends &extends) = 0;
};

class HorizontalTilingLayout : public LinearTilingLayout {
    public:
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
};
*/

class DoubleCollumnTilingLayout : public TilingLayout<std::vector<Surface::Toplevel*>::iterator> {
    std::vector<Surface::Toplevel*> surfaces;
    uint middle_index = 0;
    uint size = 0;

    public:
    void addSurface(Surface::Toplevel* surface) {
        assert(middle_index <= size);
        assert(size == surfaces.size()); //only debug
        bool second = middle_index > size / 2;
        if(second) {
            surfaces.push_back(surface);
        } else {
            surfaces.insert(surfaces.begin() + middle_index, surface);
            middle_index++;
        }
        size++;
    }

    Surface::Toplevel* removeSurface(Surface::Toplevel* surface) {
        assert(size != 0);
        auto it = std::find(surfaces.begin(), surfaces.end(), surface);
        assert(it != surfaces.end());

        if(it < surfaces.begin() + middle_index) middle_index--;

        it = surfaces.erase(it);

        size--;
        if(surfaces.begin() == surfaces.end()) return nullptr;
        if(it != surfaces.end()) return *it;
        else return *surfaces.begin();

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

    Iterator begin() {
        return surfaces.begin();
    }

    Iterator end() {
        return surfaces.end();
    }

    void updateCollumn(Extends extends, uint start, uint end, std::function<void(Surface::Toplevel*, Extends)> set_extends) {
        const uint n = end - start;
        debug("Update layout: n={}; Extends={}", n, extends);
        if(n == 0) return;
        const uint h = extends.height / n;
        auto i = start;
        Extends ext = extends;
        ext.height = extends.height - (n-1)*h;
        debug("Update layout surface: Extends={}", ext);
        set_extends(surfaces[i], ext);
        ext.y += ext.height;
        ext.height = h;
        i++;
        while(i < end) {
            debug("Update layout surface: Extends={}", ext);
            set_extends(surfaces[i], ext);
            ext.y += h;
            i++;
        }
    }

    void updateLayout(Extends &extends, std::function<void(Surface::Toplevel*, Extends)> set_extends) {
        if(size == 0) return;
        for(auto s : surfaces) s->setAvailableArea(extends); //TODO: suboptimal das musi zu desktop schiebe
        int m = extends.width / 2;
        if(middle_index == size) m = extends.width;
        else if(middle_index == 0) m = 0;
        updateCollumn({
            extends.x,
            extends.y,
            m,
            extends.height
        }, 0, middle_index, set_extends);
        updateCollumn({
            extends.x + m,
            extends.y,
            extends.width - m,
            extends.height
        }, middle_index, size, set_extends);
    };

    using TilingLayout::updateLayout;
};

class Desktop {
    DoubleCollumnTilingLayout tilingLayout;
    Extends extends;
    Surface::Toplevel* focused_toplevel = nullptr;
    Surface::Toplevel* maximized_toplevel = nullptr;
    Surface::Toplevel* fullscreen_toplevel = nullptr;
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
        if(maximized_toplevel) {
            for(auto it = tilingLayout.begin(); it != tilingLayout.end(); it++) {
                if(*it!=maximized_toplevel) (*it)->setVisibility(true);
            }
            maximized_toplevel = nullptr;
        }
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

    //FIXME: this is all horrible and duplicated code#
    void toggleMaximize() {
        if(!maximized_toplevel) {
            //MAXIMIZE
            if(!focused_toplevel) return;
            maximized_toplevel = focused_toplevel;
            for(auto it = tilingLayout.begin(); it != tilingLayout.end(); it++) {
                if(*it!=maximized_toplevel) (*it)->setVisibility(false);
            }
            maximized_toplevel->setExtends(extends);

        } else {
            //UNMAXIMIZE
            for(auto it = tilingLayout.begin(); it != tilingLayout.end(); it++) {
                if(*it!=maximized_toplevel) (*it)->setVisibility(true);
            }
            tilingLayout.updateLayout(extends);
            maximized_toplevel = nullptr;
        }
    }

    void toggleFullscreen(Extends full_ext) {
        if(!fullscreen_toplevel) {
            //FULLSCREEN
            if(!focused_toplevel) return;
            fullscreen_toplevel = focused_toplevel;
            for(auto it = tilingLayout.begin(); it != tilingLayout.end(); it++) {
                if(*it!=fullscreen_toplevel) (*it)->setVisibility(false);
            }
            fullscreen_toplevel->setFullscreen(true, full_ext);
        } else {
            //UNFULLSCREEN
            for(auto it = tilingLayout.begin(); it != tilingLayout.end(); it++) {
                if(*it!=fullscreen_toplevel) (*it)->setVisibility(true);
            }
            const Surface::Toplevel* f = fullscreen_toplevel;
            tilingLayout.updateLayout(extends, [f](Surface::Toplevel* toplevel, Extends ext){
            if(toplevel!=f)toplevel->setExtends(ext);
            else toplevel->setFullscreen(false, ext);
            });
            fullscreen_toplevel = nullptr;
        }
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

    void toggleMaximize() {
        desktops[current_desktop].toggleMaximize();
    }

    void toggleFullscreen() {
        desktops[current_desktop].toggleFullscreen(extends);
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

    Extends getExtends() {
        return extends;
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

void toggleMaximize() {
    getFocusedDisplay()->toggleMaximize();
}

void toggleFullscreen() {
    getFocusedDisplay()->toggleFullscreen();
}

Extends getActiveDisplayDimensions() {
    return getFocusedDisplay()->getExtends();
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