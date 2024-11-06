#ifndef VITOWM_SURFACE_H
#define VITOWM_SURFACE_H

#include "includes.hpp"
#include <utility>

class Surface {
    public:
    virtual void setFocused() = 0;
    virtual wlr_surface* getSurface() = 0;
    bool contains(int x, int y);

    protected:

    struct wlr_box extends; //size of the window including the borders
    struct wlr_scene_tree* root_node; //Root node of the Window. It is the parent of the surface and the borders
};

class ToplevelSurface : public Surface {
    public:
    void setExtends(struct wlr_box extends);
    void setFocused();
    ToplevelSurface();
    virtual ~ToplevelSurface();
    std::pair<int, int> surfaceCoordinateTransform(int x, int y);
    void map_notify();
    void unmap_notify();

    protected:
    virtual void setSurfaceSize(uint width, uint height) = 0;
    virtual void setActivated(bool activated) = 0; 

    struct wlr_scene_tree* surface_node; //Surface node: holds the surface itself without decorations
	struct wlr_scene_rect* border[4]; // Borders of the window in the following order: top, bottom, left, right
};

class XdgToplevelSurface : public ToplevelSurface {

    public:
    XdgToplevelSurface(wlr_xdg_toplevel* xdg_toplevel);
    ~XdgToplevelSurface();

    private:
    void setSurfaceSize(uint width, uint height);
    void setActivated(bool activated);
    wlr_surface* getSurface();

    struct wlr_xdg_toplevel* xdg_toplevel;
    //struct wlr_xdg_surface* xdg_surface;
};

void setupSurface();

#endif