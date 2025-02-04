#pragma once

#include "includes.hpp"
#include "util.hpp"
#include <utility>
#include <set>

namespace Surface {
    class Base {
        public:
        void setExtends(wlr_box extends);
        bool contains(int x, int y);

        virtual void setFocus(bool focus) = 0;
        virtual wlr_surface* getSurface() = 0;
        virtual std::pair<int, int> surfaceCoordinateTransform(int x, int y) const = 0;

        //ich will das eig nöd public
        wlr_scene_tree* root_node = nullptr; //Root node of the Window. It is the parent of the surface and possible borders
        protected:
        Extends extends = {0,0,0,0}; //size of the window including the borders

        virtual void extendsUpdateNotify(bool resize) = 0;
    };

    class Child;

    class Parent : public Base{
        std::set<Child*> children;

        public:
        wlr_scene_tree* addChild(Child* child);
        void removeChild(Child* child);
        void arrangeChildren(Extends ext, int x, int y);
        virtual void arrangeAll() = 0;
        bool contains(int x, int y, bool include_children);
        using Base::contains;

        virtual Point getGlobalOffset() = 0;
        virtual Extends& getAvailableArea() = 0;
    };

    //child surface of some parent
	//child surfaces can be parents of other children
	class Child : public Parent {
		protected:
		Parent* parent;

		public:
		Child(Parent* parent) : parent(parent) {}
		virtual ~Child() = default;
		virtual void arrange(Extends ext, int x, int y) = 0; //extends of available space
		void arrangeAll();
        
        Point getGlobalOffset();
        Extends& getAvailableArea();
	};

    class Toplevel : public Parent {
        public:
        Toplevel();
        virtual ~Toplevel() = default;
        
        void setFocus(bool focus);
        void setVisibility(bool visible);
        std::pair<int, int> surfaceCoordinateTransform(int x, int y) const;
        void setChildExtends(Extends* ext);
        void arrangeAll();

        Point getGlobalOffset();
        Extends& getAvailableArea();

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

        Extends* child_ext;
    };

    void setupXdgShell();
    void setupXWayland();
}
