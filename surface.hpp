#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <utility>

namespace Surface {
    class Base {
        public:
        virtual void setFocus(bool focus) = 0;  //TODO: da setFocused(bool) und focus bi layout ane tue
        virtual wlr_surface* getSurface() = 0;
        bool contains(int x, int y);
        virtual std::pair<int, int> surfaceCoordinateTransform(int x, int y) = 0;

        protected:

        Extends extends; //size of the window including the borders
        wlr_scene_tree* root_node; //Root node of the Window. It is the parent of the surface and the borders
    };

    class Toplevel : public Base {
        public:
        void setExtends(struct wlr_box extends);
        void setFocus(bool focus);
        Toplevel();
        virtual ~Toplevel();
        std::pair<int, int> surfaceCoordinateTransform(int x, int y);
        void map_notify();
        void unmap_notify();

        protected:
        virtual void setSurfaceSize(uint width, uint height) = 0;
        virtual void setActivated(bool activated) = 0; 

        struct wlr_scene_tree* surface_node; //Surface node: holds the surface itself without decorations
        struct wlr_scene_rect* border[4]; // Borders of the window in the following order: top, bottom, left, right
    };

    class XdgToplevel : public Toplevel {

        public:
        XdgToplevel(wlr_xdg_toplevel* xdg_toplevel);
        ~XdgToplevel();

        private:
        void setSurfaceSize(uint width, uint height);
        void setActivated(bool activated);
        wlr_surface* getSurface();

        struct wlr_xdg_toplevel* xdg_toplevel;
        //struct wlr_xdg_surface* xdg_surface;
    };

    void setup();
}
