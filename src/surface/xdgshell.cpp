#define LOGGER_CATEGORY Logger::SURFACE
#include "surface.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include "server.hpp"

#include <wlr/util/log.h>
#include <cassert>
#include <map>
#include <set>

namespace Surface {

class XdgToplevel : public Toplevel {
    struct wlr_xdg_toplevel* xdg_toplevel;

    void setSurfaceSize(uint width, uint height) {
        wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);
    }

    void setActivated(bool activated) {
        wlr_xdg_toplevel_set_activated(xdg_toplevel, activated);
    }

    wlr_surface* getSurface() {
        return xdg_toplevel->base->surface;
    }

    public:
    XdgToplevel(wlr_xdg_toplevel* xdg_toplevel) {
        this->xdg_toplevel = xdg_toplevel;
        xdg_toplevel->base->surface->data = this;
        root_node = wlr_scene_tree_create(&Output::scene->tree);
        surface_node = wlr_scene_xdg_surface_create(root_node, xdg_toplevel->base);
    }

    ~XdgToplevel() {
        wlr_scene_node_destroy(&root_node->node);
    }

    void map_event(bool map) {
        mapNotify(map);
    }
};

wlr_xdg_shell *xdg_shell;
wl_listener new_xdg_toplevel;
wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
wl_listener xdg_decoration_listener;
wl_listener new_xdg_popup;

struct xdg_shell_surface_listeners {
    XdgToplevel* surface;

    wl_listener map_listener;
    wl_listener unmap_listener;
    wl_listener destroy_listener;
    wl_listener commit;

    wlr_xdg_toplevel* xdg_toplevel;
};

void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
    wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
    xdg_shell_surface_listeners* listeners = new xdg_shell_surface_listeners();
    listeners->xdg_toplevel = xdg_toplevel;
    listeners->surface = new XdgToplevel(xdg_toplevel);
    xdg_toplevel->base->data = nullptr;

            //CONFIGURE LISTENERS
    listeners->map_listener.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, map_listener);
        listeners->surface->map_event(true);
    };

    listeners->unmap_listener.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, unmap_listener);
        listeners->surface->map_event(false);
    };

    listeners->destroy_listener.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, destroy_listener);
        delete listeners->surface;

        wl_list_remove(&listeners->map_listener.link);
        wl_list_remove(&listeners->unmap_listener.link);
        wl_list_remove(&listeners->destroy_listener.link);

        delete listeners;
    };

    wl_signal_add(&xdg_toplevel->base->surface->events.map, 		&listeners->map_listener);
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, 		&listeners->unmap_listener);
    wl_signal_add(&xdg_toplevel->base->surface->events.destroy, 	&listeners->destroy_listener);

    listeners->commit.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, commit);
        wlr_xdg_toplevel* toplevel = listeners->xdg_toplevel;

        if(!toplevel->base->initial_commit) return;
        assert(toplevel->base->initialized);

        wlr_xdg_toplevel_decoration_v1* dec = (wlr_xdg_toplevel_decoration_v1*) toplevel->base->data;
        if(!dec) return; //TODO: add full support for fullscreen: okular presentation mode
        dec->scheduled_mode = WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        wlr_xdg_surface_schedule_configure(toplevel->base);
    };
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, 	&listeners->commit);
}

void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
    struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
    dec->toplevel->base->data = dec;
}

class XdgPopup : public Child {
    wlr_xdg_popup* popup;

    public:
    XdgPopup(wlr_xdg_popup* popup) : Child((Parent*)popup->parent->data), popup(popup) {
        const auto parent_scene_tree = parent->addChild(this);
        root_node = wlr_scene_xdg_surface_create(parent_scene_tree, popup->base);
        popup->base->surface->data = this;
    }

    ~XdgPopup() {
        assert(parent);
        parent->removeChild(this);
        wlr_scene_node_destroy(&root_node->node);
    }

    void arrange(Extends ext, int x, int y) {
        ext.x -= x;
        ext.y -= y;
        extends = Extends(popup->scheduled.geometry).constrain(ext);
    }

    void position() {
        //FIXME: for a few ms the original postion shows up
        //		 idk if this is fixable without replacing wlroots
        popup->scheduled.geometry = extends;
        wlr_xdg_surface_schedule_configure(popup->base);
    }

    //TODO: restructure inheritance: das züg bruchts alles nöd
    void setFocus(bool focus) {}
    wlr_surface* getSurface() {return nullptr;}
    std::pair<int, int> surfaceCoordinateTransform(int x, int y) const {return {0,0};}
    void extendsUpdateNotify(bool resize) {}
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
        // FIXME: this crashes idk why
        // wl_list_remove(&listeners->destroy.link);
        // wl_list_remove(&listeners->reposition.link);
        // wl_list_remove(&listeners->commit.link);
        delete listeners->popup;
        delete listeners;
    };
    wl_signal_add(&xdg_popup->events.destroy, &listeners->destroy);

    listeners->reposition.notify = [](wl_listener* listener, void* data) {
        xdg_popup_listeners* listeners = wl_container_of(listener, listeners, reposition);
        listeners->popup->position();
    };
    wl_signal_add(&xdg_popup->events.destroy, &listeners->destroy);

    listeners->commit.notify = [](wl_listener* listener, void* data) {
        xdg_popup_listeners* listeners = wl_container_of(listener, listeners, commit);
        listeners->popup->position();
    };
    wl_signal_add(&xdg_popup->base->surface->events.commit, &listeners->commit);
}

void setupXdgShell() {
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

}