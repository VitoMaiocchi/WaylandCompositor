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

struct xdg_shell_surface_listeners {
    XdgToplevel* surface;

    wl_listener map;
    wl_listener unmap;
    wl_listener destroy;
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
    listeners->map.notify = [](struct wl_listener* listener, void* data) {
        xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, map);
        listeners->surface->map_event(true);
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

        delete listeners;
    };

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


#include "xdg-shell-protocol.h"

namespace { //TODO: XDG SHELL FROM SCRATCH

    struct XdgClient {
        wl_resource* xdg_wm_base;
        wl_client* client;
    };

    static void xdg_shell_handle_create_positioner(struct wl_client *wl_client,
            struct wl_resource *resource, uint32_t id) {
        debug("xdg_shell_create_positioner: id={}",id);
        // struct wlr_xdg_client *client =
        //     xdg_client_from_resource(resource);
        // create_xdg_positioner(client, id);
    }

    static void xdg_shell_handle_get_xdg_surface(struct wl_client *wl_client,
            struct wl_resource *xdg_wm_base, uint32_t id,
            struct wl_resource *surface_resource) { //existing surface that the client wants to turn in to a xdg surface
        debug("xdg_shell_get_surface: id={}",id);
        // struct wlr_xdg_client *client =
        //     xdg_client_from_resource(client_resource);
        // struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
        // create_xdg_surface(client, surface, id);
    }

    static void xdg_shell_handle_pong(struct wl_client *wl_client,
            struct wl_resource *resource, uint32_t serial) {
        debug("handle pong");
        // struct wlr_xdg_client *client = xdg_client_from_resource(resource);

        // if (client->ping_serial != serial) {
        //     return;
        // }

        // wl_event_source_timer_update(client->ping_timer, 0);
        // client->ping_serial = 0;
    }

    static void xdg_shell_handle_destroy(struct wl_client *wl_client,
            struct wl_resource *resource) {
        debug("xdg_shell_resource_destroy");
        // struct wlr_xdg_client *client = xdg_client_from_resource(resource);

        // if (!wl_list_empty(&client->surfaces)) {
        //     wl_resource_post_error(client->resource,
        //         XDG_WM_BASE_ERROR_DEFUNCT_SURFACES,
        //         "xdg_wm_base was destroyed before children");
        //     return;
        // }

        wl_resource_destroy(resource);
    }


    struct {
        wl_global* global;
        wl_listener display_destroy;
        std::set<XdgClient*> clients;

        struct xdg_wm_base_interface implementation = {
        .destroy = xdg_shell_handle_destroy,
        .create_positioner = xdg_shell_handle_create_positioner,
        .get_xdg_surface = xdg_shell_handle_get_xdg_surface,
        .pong = xdg_shell_handle_pong,
        };

    } xdgShell;

    static void destroy_xdg_wm_base(wl_resource *resource) {
        XdgClient* client = (XdgClient*) wl_resource_get_user_data(resource);
        assert(client);

        //TODO: destroy all surfaces 

        auto it = xdgShell.clients.find(client);
        assert(it != xdgShell.clients.end());
        xdgShell.clients.erase(it);
        delete client;
    }

    static void xdgShellBind(wl_client *wl_client, void *data, uint32_t version, uint32_t id) {
        info("new xdg client: id={}", id);
        XdgClient* client = new XdgClient;

        client->client = wl_client;
        client->xdg_wm_base = wl_resource_create(wl_client, &xdg_wm_base_interface, version, id);
        if (client->xdg_wm_base == NULL) {
            wl_client_post_no_memory(wl_client);
            return;
        }

        wl_resource_set_implementation(client->xdg_wm_base, &xdgShell.implementation, client,
            destroy_xdg_wm_base);

        xdgShell.clients.insert(client);
    }

    void displayDestroyNotify(struct wl_listener *listener, void *data) {
        wl_global_destroy(xdgShell.global);
        //TODO display destroy
    }

    void createXdgShell(wl_display *display, uint32_t version) { 
        //ping timeout 10000

        xdgShell.global = wl_global_create(display, &xdg_wm_base_interface, version, nullptr, xdgShellBind);
        if(!xdgShell.global) {
            error("failed to create xdg shell");
            return;
        }

        xdgShell.display_destroy.notify = displayDestroyNotify;
        wl_display_add_destroy_listener(display, &xdgShell.display_destroy);
    }

    void setupXdgShell_fromScratch() {
        createXdgShell(Server::display, 3);
    }

}

void Surface::setupXdgShell() {
    setupXdgShell_fromScratch();
    // xdg_shell = wlr_xdg_shell_create(Server::display, 3);
    // new_xdg_toplevel.notify = new_xdg_toplevel_notify;
    // new_xdg_popup.notify = new_xdg_popup_notify;
    // wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);	
    // wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    // //requesting serverside decorations of clients
    // wlr_server_decoration_manager_set_default_mode(
    //         wlr_server_decoration_manager_create(Server::display),
    //         WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
    // xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(Server::display);
    // xdg_decoration_listener.notify = xdg_new_decoration_notify;
    // wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration, &xdg_decoration_listener);
}