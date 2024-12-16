#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <utility>

namespace Surface {
    class Base {
        public:
        void setExtends(wlr_box extends);
        bool contains(int x, int y);

        virtual void setFocus(bool focus) = 0;
        virtual wlr_surface* getSurface() = 0;
        virtual std::pair<int, int> surfaceCoordinateTransform(int x, int y) = 0;

        protected:
        Extends extends; //size of the window including the borders
        wlr_scene_tree* root_node; //Root node of the Window. It is the parent of the surface and possible borders

        virtual void extendsUpdateNotify(bool resize) = 0;
    };

    class Toplevel : public Base {
        public:
        Toplevel();
        virtual ~Toplevel() = default;
        
        void setFocus(bool focus);
        void setVisibility(bool visible);
        std::pair<int, int> surfaceCoordinateTransform(int x, int y);

        protected:
        virtual void setSurfaceSize(uint width, uint height) = 0;
        virtual void setActivated(bool activated) = 0; 

        wlr_scene_tree* surface_node; //Surface node: holds the surface itself without decorations
        wlr_scene_rect* border[4]; // Borders of the window in the following order: top, bottom, left, right

        void mapNotify(bool mapped);
        void minimizeRequestNotify(bool minimize); //TODO minimize support
        void fullscreenRequestNotify(bool fullscreen); //TODO fullscreen support

        private:
        void extendsUpdateNotify(bool resize);

        bool visible; //visibility determined by layout
        bool maximized;
        bool focused;

        bool mapped;
        bool minimized;
        bool fullscreen;
    };

    void setup();
}
