#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <utility>
#include <set>

namespace Surface {
    class Base {
        public:
        bool contains(int x, int y);

        protected:
        wlr_scene_tree* root_node = nullptr; //Root node of the Window. It is the parent of the surface and possible borders
        Extends extends = {0,0,0,0}; //size of the window including the borders
    };

    class Child;

    class Parent : public Base{
        friend Child;
        std::set<Child*> children;

        virtual Point getGlobalOffset() = 0;
        virtual Extends& getAvailableArea() = 0;

        wlr_scene_tree* addChild(Child* child);
        void removeChild(Child* child);

        public:
        bool contains(int x, int y, bool include_children);
        using Base::contains;
    };

    //child surface of some parent
	//child surfaces can be parents of other children
	class Child : public Parent {
		protected:
		Parent* parent;
        wlr_scene_tree* parent_root;

		Child(Parent* parent);
		virtual ~Child();
        
        Point getGlobalOffset();
        Extends& getAvailableArea();
	};

    class Toplevel : public Parent {
        public:
        void setExtends(Extends extends);
        void setFocus(bool focus);
        void setVisibility(bool visible);

        void setAvailableArea(Extends ext);
        std::pair<int, int> surfaceCoordinateTransform(int x, int y) const;
        virtual wlr_surface* getSurface() = 0;

        virtual void kill() = 0;

        protected:
        Toplevel();
        virtual ~Toplevel() = default;

        virtual void setSurfaceSize(uint width, uint height) = 0;
        virtual void setActivated(bool activated) = 0; 

        void mapNotify(bool mapped);
        void minimizeRequestNotify(bool minimize); //TODO minimize support
        void fullscreenRequestNotify(bool fullscreen); //TODO fullscreen support

        wlr_scene_tree* surface_node; //Surface node: holds the surface itself without decorations
        wlr_scene_rect* border[4]; // Borders of the window in the following order: top, bottom, left, right

        private:
        Point getGlobalOffset();
        Extends& getAvailableArea();

        bool visible; //visibility determined by layout
        bool maximized;
        bool focused;

        bool mapped;
        bool minimized;
        bool fullscreen;

        Extends availableArea;
    };

    void setupXdgShell();
    void setupXWayland();
}
