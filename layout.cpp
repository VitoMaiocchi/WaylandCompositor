#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include <list>
#include <algorithm>
#include <cassert>

namespace Layout {

    //DAS NODE ZÜG BINI NONIG SICHER OBS GUET ISCH
    //abstract base class for different layout types 
    class Base {
        public:
            Base(Extends ext);
            virtual ~Base() = default;
            void updateExtends(Extends ext);
            virtual void addSurface(Surface::Toplevel* surface) = 0;
            virtual Surface::Toplevel* removeSurface(Surface::Toplevel* surface) = 0;

        protected:
            Extends extends;
            virtual void updateLayout() = 0;
    };

    Base::Base(Extends ext) : extends(ext) {}

    void Base::updateExtends(Extends ext) {
        extends = ext;
        updateLayout();
    }

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

    //END NODE ZÜG

    //FIXME: temorary workaround. Ich burch e besseri extends class
    Extends t_height(Extends ext) {
        ext.height = 30;
        return ext;
    }

    struct Fullscreen {
        Extends extends;
        Titlebar titlebar;
        std::unique_ptr<Base> node;

        Fullscreen(Extends ext) : extends(ext), titlebar(t_height(ext)) {
            Extends layout_ext = ext;
            layout_ext.height -= 30;
            layout_ext.y += 30;
            node = std::make_unique<Linear>(layout_ext);
        }

        void updateExtends(Extends ext) {
            extends = ext;
            Extends layout_ext = ext;
            layout_ext.height -= 30;
            layout_ext.y += 30;
            node->updateExtends(layout_ext);
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
    };

    Surface::Toplevel* focused_toplevel;

    std::list<Display*> displays;
    Display* focused_display = nullptr;

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

    inline Display* getFocusedDisplay() {
        assert(displays.size() > 0);
        if(!focused_display) focused_display = *displays.begin();
        return focused_display;
    }

    void addSurface(Surface::Toplevel* surface) {
        debug("ADD SURFACE");
        getFocusedDisplay()->fullscreen->node->addSurface(surface);
        setFocus(surface);
        debug("ADD SURFACE end");
    }

    void removeSurface(Surface::Toplevel* surface) {
        auto next = getFocusedDisplay()->fullscreen->node->removeSurface(surface);
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
        dynamic_cast<Linear*>(getFocusedDisplay()->fullscreen->node.get())->forEach([&surface, x, y](Surface::Toplevel* s){
            if(s->contains(x,y)) surface = s;
        });

        setFocus(surface);
        Input::setCursorFocus(surface);

        for(Display* display : displays) if(display->fullscreen->contains(x,y)) focused_display = display;
    }

    void setDesktop(uint desktop) {
        getFocusedDisplay()->fullscreen->titlebar.updateDesktop(desktop);
    }

    Display::Display(Extends ext) {
        displays.push_back(this);
        fullscreen = std::make_unique<Fullscreen>(ext);
    }

    Display::~Display() {
        displays.remove(this);
        if(focused_display == this) focused_display = nullptr;
    }

    void Display::updateExtends(Extends ext) {
        fullscreen->updateExtends(ext);
    }

}