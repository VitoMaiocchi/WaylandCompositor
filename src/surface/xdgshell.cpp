#define LOGGER_CATEGORY Logger::SURFACE
#include "surface.hpp"
#include "server.hpp"
#include "output.hpp"

#include <cassert>

namespace {

    //TOPLEVEL

class XdgToplevel : public Surface::Toplevel {
    struct wlr_xdg_toplevel* xdg_toplevel;

    void setSurfaceSize(uint width, uint height) {
        assert(width >= 0 && height >= 0);
        xdg_toplevel->scheduled.width = width;
        xdg_toplevel->scheduled.height = height;
        wlr_xdg_surface_schedule_configure(xdg_toplevel->base);
    }

    void setActivated(bool activated) {
        wlr_xdg_toplevel_set_activated(xdg_toplevel, activated);
    }

    wlr_surface* getSurface() {
        return xdg_toplevel->base->surface;
    }

    void kill() {
        wlr_xdg_toplevel_send_close(xdg_toplevel);
    }

    public:
    XdgToplevel(wlr_xdg_toplevel* xdg_toplevel) {
        debug("NEW XDG TOPLEVEL");
        this->xdg_toplevel = xdg_toplevel;
        xdg_toplevel->base->surface->data = this;
        surface_node = wlr_scene_xdg_surface_create(root_node, xdg_toplevel->base);
    }

    ~XdgToplevel() {
        wlr_scene_node_destroy(&root_node->node);
    }

    void map_event(bool map) {
        mapNotify(map);
    }

    std::pair<int, int> surfaceCoordinateTransform(int x, int y) const {
        wlr_box box;
        wlr_xdg_surface_get_geometry(xdg_toplevel->base, &box);
        auto p = Surface::Toplevel::surfaceCoordinateTransform(x,y);
        p.first += box.x;
        p.second += box.y;
        return p;
    }

    void setFullscreen(bool fullscreen) {
        wlr_xdg_toplevel_set_fullscreen(xdg_toplevel, fullscreen);
    }
};

struct xdg_shell_surface_listeners {
    XdgToplevel* surface;

    wl_listener map;
    wl_listener unmap;
    wl_listener destroy;
    wl_listener commit;

    wlr_xdg_toplevel* xdg_toplevel;
    bool innitial_commit = true;
};

void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
    wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
    xdg_shell_surface_listeners* listeners = new xdg_shell_surface_listeners();
    listeners->xdg_toplevel = xdg_toplevel;
    listeners->surface = new XdgToplevel(xdg_toplevel);
    xdg_toplevel->base->data = nullptr;

            //CONFIGURE LISTENERS
    listeners->map.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, map);
        //listeners->surface->map_event(true);
        //TODO: clean this up
    };

    listeners->unmap.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, unmap);
        listeners->surface->map_event(false);
    };

    listeners->destroy.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, destroy);
        delete listeners->surface;

        wl_list_remove(&listeners->map.link);
        wl_list_remove(&listeners->unmap.link);
        wl_list_remove(&listeners->destroy.link);
        wl_list_remove(&listeners->commit.link);

        delete listeners;
    };

    listeners->commit.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, commit);
        wlr_xdg_toplevel* toplevel = listeners->xdg_toplevel;

        if(!listeners->innitial_commit) return;
        if(!toplevel->base->initial_commit) return;
        assert(toplevel->base->initialized);

        listeners->surface->map_event(true);
        listeners->innitial_commit= false;

        wlr_xdg_toplevel_decoration_v1* dec = (wlr_xdg_toplevel_decoration_v1*) toplevel->base->data;
        if(!dec) return; //TODO: add full support for fullscreen: okular presentation mode
        dec->scheduled_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        wlr_xdg_surface_schedule_configure(toplevel->base);
    };

    wl_signal_add(&xdg_toplevel->base->surface->events.map, 		&listeners->map);
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, 		&listeners->unmap);
    wl_signal_add(&xdg_toplevel->base->surface->events.destroy, 	&listeners->destroy);
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, 	    &listeners->commit);
}

void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
    dec->toplevel->base->data = dec;
}




    //POPUP

class XdgPopup : public Surface::Child {
    wlr_xdg_popup* popup;

    public:
    XdgPopup(wlr_xdg_popup* popup) : Child((Parent*)popup->parent->data), popup(popup) {
        assert(parent_root);
        root_node = wlr_scene_xdg_surface_create(parent_root, popup->base);
        popup->base->surface->data = this;
    }

    ~XdgPopup() {
        wlr_scene_node_destroy(&root_node->node);
    }

    void position() {
        //FIXME: for a few ms the original postion shows up
        //		 idk if this is fixable without replacing wlroots

        extends.x = 0;
        extends.y = 0;
        Point globalOffset = getGlobalOffset();

        Extends ext = getAvailableArea();
        ext.x -= globalOffset.x;
        ext.y -= globalOffset.y;

        extends = Extends(popup->scheduled.geometry).constrain(ext);
        popup->scheduled.geometry = extends;
        wlr_xdg_surface_schedule_configure(popup->base);
    }
};

struct xdg_popup_listeners {
    XdgPopup* popup = nullptr;
    wl_listener destroy;
    wl_listener reposition;
    wl_listener commit;
};

void new_xdg_popup_notify(struct wl_listener* listener, void* data) {
    wlr_xdg_popup* xdg_popup = (wlr_xdg_popup*) data;
    if(!xdg_popup->parent) { //maybe make this assert idk
        return;
    }

    xdg_popup_listeners* listeners = new xdg_popup_listeners();
    listeners->popup = new XdgPopup(xdg_popup);
    
    listeners->destroy.notify = [](wl_listener* listener, void* data) {
        xdg_popup_listeners* listeners = wl_container_of(listener, listeners, destroy);
        wl_list_remove(&listeners->destroy.link);
        wl_list_remove(&listeners->reposition.link);
        wl_list_remove(&listeners->commit.link);
        delete listeners->popup;
        delete listeners;
    };

    listeners->reposition.notify = [](wl_listener* listener, void* data) {
        xdg_popup_listeners* listeners = wl_container_of(listener, listeners, reposition);
        listeners->popup->position();
    };

    listeners->commit.notify = [](wl_listener* listener, void* data) {
        xdg_popup_listeners* listeners = wl_container_of(listener, listeners, commit);
        listeners->popup->position();
    };

    wl_signal_add(&xdg_popup->events.destroy, &listeners->destroy);
    wl_signal_add(&xdg_popup->events.reposition, &listeners->reposition);
    wl_signal_add(&xdg_popup->base->surface->events.commit, &listeners->commit);
}



wlr_xdg_shell *xdg_shell;
wl_listener new_xdg_toplevel;
wl_listener new_xdg_popup;
wl_listener xdg_decoration_listener;
wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;

}

void Surface::setupXdgShell() {
    xdg_shell = wlr_xdg_shell_create(Server::display, 3);
    new_xdg_toplevel.notify = new_xdg_toplevel_notify;
    new_xdg_popup.notify = new_xdg_popup_notify;
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);	
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    //requesting serverside decorations of clients
    wlr_server_decoration_manager_set_default_mode(
            wlr_server_decoration_manager_create(Server::display),
            WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(Server::display);
    xdg_decoration_listener.notify = xdg_new_decoration_notify;
    wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration, &xdg_decoration_listener);
}