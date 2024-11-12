#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include <list>
#include <algorithm>
#include <cassert>

namespace Layout {

    struct Linear : public Base {
        using Base::Base;

        std::list<Surface::Toplevel*> surfaces;

        void updateLayout() override {
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

        void addSurface(Surface::Toplevel* surface) override {
            surfaces.push_back(surface);
            updateLayout();
        };

        Surface::Toplevel* removeSurface(Surface::Toplevel* surface) override {
            auto it = std::find(surfaces.begin(), surfaces.end(), surface);
            assert(it != surfaces.end());

            it = surfaces.erase(it);
            updateLayout();

            if(surfaces.begin() == surfaces.end()) return nullptr;
            if(it != surfaces.end()) return *it;
            else return *surfaces.begin();
        };

        void forEach(std::function<void(Surface::Toplevel* toplevel)> func) {
            for(auto surface : surfaces) func(surface);
        }
    };

    Surface::Toplevel* focused_toplevel;
    Output::Display* focused_display;

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

    inline Output::Display* getFocusedDisplay() {
        if(!focused_display) focused_display = *Output::displays.begin();
        assert(focused_display);
        return focused_display;
    }

    void addSurface(Surface::Toplevel* surface) {
        debug("ADD SURFACE");
        getFocusedDisplay()->layout->addSurface(surface);
        setFocus(surface);
        debug("ADD SURFACE end");
    }

    void removeSurface(Surface::Toplevel* surface) {
        auto next = getFocusedDisplay()->layout->removeSurface(surface);
        if(surface == focused_toplevel) {
            if(!next) {
                focused_toplevel = nullptr;
                return;
            }
            focused_toplevel = next;
            focused_toplevel->setFocus(true);
        }
    }

    void handleCursorMovement(const double x, const double y) {
        Surface::Toplevel* surface = nullptr;
        dynamic_cast<Linear*>(getFocusedDisplay()->layout.get())->forEach([&surface, x, y](Surface::Toplevel* s){
            if(s->contains(x,y)) surface = s;
        });
        //if(!surface) return;
        setFocus(surface);
        Input::setCursorFocus(surface);
    }

    Base::Base(Extends ext) : extends(ext) {}

    void Base::updateExtends(Extends ext) {
        extends = ext;
        updateLayout();
    }

    void removeDisplay(Output::Display* display) {
        if(focused_display == display) focused_display = nullptr;
    }

    std::unique_ptr<Base> generateNewLayout(Extends ext) {
        return std::make_unique<Linear>(ext);
    }
}