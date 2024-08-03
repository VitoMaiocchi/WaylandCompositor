
#include "includes.hpp"

class Surface {
    protected:
    struct wlr_box extends; //size of the window including the borders
    struct wlr_scene_tree* root_node; //Root node of the Window. It is the parent of the surface and the borders
};

class ToplevelSurface : public Surface {
    public:
    void setExtends(struct wlr_box extends);
    ToplevelSurface();

    protected:
    virtual void setSurfaceSize(uint width, uint height) = 0;
    void map_notify();
    void unmap_notify();

    struct wl_listener map_listener;
	struct wl_listener unmap_listener;
	struct wl_listener destroy_listener;

    struct wlr_scene_tree* surface_node; //Surface node: holds the surface itself without decorations
	struct wlr_scene_rect* border[4]; // Borders of the window in the following order: top, bottom, left, right
};

class XdgToplevelSurface : public ToplevelSurface {

    public:
    XdgToplevelSurface(wlr_xdg_toplevel* xdg_toplevel);
    ~XdgToplevelSurface();

    private:
    void setSurfaceSize(uint width, uint height);

    struct wlr_xdg_toplevel* xdg_toplevel;
    //struct wlr_xdg_surface* xdg_surface;
};