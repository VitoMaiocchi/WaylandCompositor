#define LOGGER_CATEGORY Logger::SURFACE
#include "surface.hpp"
#include "input.hpp"
#include "output.hpp"
#include "server.hpp"

#include <cassert>
#include <map>
#include <set>

namespace {

class XwaylandToplevel : public Surface::Toplevel {
    wlr_xwayland_surface* xwayland_surface;

    public:
    XwaylandToplevel(wlr_xwayland_surface* xwayland_surface) {
        debug("NEW XWAYLAND TOPLEVEL");
        this->xwayland_surface = xwayland_surface;
        xwayland_surface->data = this;
    }

    void map(bool map) {
        if(map) surface_node = wlr_scene_subsurface_tree_create(root_node, xwayland_surface->surface);
        mapNotify(map);
    }

    ~XwaylandToplevel() {
        wlr_scene_node_destroy(&root_node->node);
    }

    private:
    void setSurfaceSize(uint width, uint height) {
        wlr_xwayland_surface_configure(xwayland_surface, extends.x, extends.y, width, height);
    }

    void setActivated(bool activated) {
        wlr_xwayland_surface_activate(xwayland_surface, activated);
    }

    wlr_surface* getSurface() {
        return xwayland_surface->surface;
    }

    void kill() {
        wlr_xwayland_surface_close(xwayland_surface);
    }

    void setFullscreen(bool fullscreen) {
        wlr_xwayland_surface_set_fullscreen(xwayland_surface, fullscreen);
    }
};

//TODO: add option for rearrange
class XwaylandPopup : public Surface::Child {
    wlr_xwayland_surface* popup;

    public:
    XwaylandPopup(wlr_xwayland_surface* popup_surface, Parent* parent) : Child(parent), popup(popup_surface) {
        assert(parent_root);
        root_node = wlr_scene_subsurface_tree_create(parent_root, popup->surface);
        popup->data = this; //das bruchts da wahrschinlich gar nöd

        auto size = popup->size_hints;
        Extends ext = getAvailableArea();
        extends.x = 0;
        extends.y = 0;
        Point globalOffset = getGlobalOffset();
        extends = Extends(size->x, size->y, size->width, size->height).constrain(ext);

        wlr_xwayland_surface_configure(popup, extends.x, extends.y, extends.width, extends.height);
        extends.x -= globalOffset.x;
        extends.y -= globalOffset.y;
        wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);
    }

    ~XwaylandPopup() {
        wlr_scene_node_destroy(&root_node->node);
        /*
        FIXME: da isch es mal abgschtürzt
        #0  0x00007ffff7ed15b3 in wlr_scene_node_destroy () from /usr/lib/libwlroots-0.18.so
        #1  0x0000555555586657 in (anonymous namespace)::XwaylandPopup::~XwaylandPopup (this=0x555556652540) at src/surface/xwayland.cpp:75
        #2  0x0000555555586689 in (anonymous namespace)::XwaylandPopup::~XwaylandPopup (this=0x555556652540) at src/surface/xwayland.cpp:74
        #3  0x00005555555868d3 in (anonymous namespace)::new_xwayland_surface_notify(wl_listener*, void*)::$_2::operator()(wl_listener*, void*) const (this=0x7fffffffe1f7, listener=0x55555663fdf0, data=0x0) at src/surface/xwayland.cpp:186
        #4  0x000055555558681e in (anonymous namespace)::new_xwayland_surface_notify(wl_listener*, void*)::$_2::__invoke(wl_listener*, void*) (listener=0x55555663fdf0, data=0x0) at src/surface/xwayland.cpp:177
        */

    }
};





enum XWaylandWindowType {
    XWAYLAND_UNASSOCIATED,
    XWAYLAND_TOPLEVEL,
    XWAYLAND_POPUP,
};

struct XWaylandSurface;
std::map<xcb_window_t, XWaylandSurface> surfaces;
XWaylandWindowType getXWaylandWindowType(wlr_xwayland_surface* surface);
XWaylandSurface* getSurface(xcb_window_t window_id, bool create);
XWaylandSurface* getLeader(xcb_window_t window_id, bool create);
Surface::Parent* getParent(xcb_window_t window);

struct XWaylandSurface {
    wlr_xwayland_surface* wlr_surface = nullptr;

    XWaylandWindowType type = XWAYLAND_UNASSOCIATED;
    union {
        XWaylandSurface* fallback = nullptr;
        XwaylandToplevel* toplevel;
        XwaylandPopup* popup;
    };

    wl_listener map;
    wl_listener associate;
    wl_listener disassociate;
    wl_listener destroy;

    bool mapped = false;
    XWaylandSurface* fallbackLeader = nullptr;

    void tryMap() {
        assert(type == XWAYLAND_TOPLEVEL);
        if(!mapped) return;
        toplevel->map(true);
    }
};

XWaylandSurface* getSurface(xcb_window_t window_id, bool create) {
    auto it = surfaces.find(window_id);
    if(it != surfaces.end()) return &it->second;
    if(!create) return nullptr;
    surfaces[window_id] = XWaylandSurface();
    return &surfaces[window_id];
}

void new_xwayland_surface_notify(struct wl_listener* listener, void* data) {
    wlr_xwayland_surface* surface = (wlr_xwayland_surface*) data;
    auto listeners = getSurface(surface->window_id, true);
    listeners->wlr_surface = surface;

    debug("new xwayland surface: window-id={}", listeners->wlr_surface->window_id);

    listeners->associate.notify = [](struct wl_listener* listener, void* data) {
        XWaylandSurface* listeners = wl_container_of(listener, listeners, associate);
        listeners->type = getXWaylandWindowType(listeners->wlr_surface);

        switch(listeners->type) {
            case XWAYLAND_TOPLEVEL:
                listeners->toplevel = new XwaylandToplevel(listeners->wlr_surface);
                listeners->tryMap();
                {
                    auto leader = getLeader(listeners->wlr_surface->window_id, true);
                    if(leader->type == XWAYLAND_UNASSOCIATED && !leader->fallback) {
                        leader->fallback = listeners;
                        listeners->fallbackLeader = leader;
                    }
                }
            break;
            case XWAYLAND_POPUP:
                listeners->popup = new XwaylandPopup(listeners->wlr_surface, getParent(listeners->wlr_surface->window_id));
            break;
            default:
            warn("xwayland surface of unknown type");
            break;
        }
    };
    wl_signal_add(&surface->events.associate, &listeners->associate);

    listeners->map.notify = [](struct wl_listener* listener, void* data) {
        XWaylandSurface* listeners = wl_container_of(listener, listeners, map);
        listeners->mapped = true;

        switch(listeners->type) {
            case XWAYLAND_TOPLEVEL:
                listeners->tryMap();
            break;
            default:
            warn("unhandled map case");
            break;
        }
    };
    wl_signal_add(&surface->events.map_request, &listeners->map);

    listeners->disassociate.notify = [](struct wl_listener* listener, void* data) {
        XWaylandSurface* listeners = wl_container_of(listener, listeners, disassociate);

        switch(listeners->type) {
            case XWAYLAND_TOPLEVEL:
                if(listeners->mapped) listeners->toplevel->map(false);
                if(listeners->fallbackLeader) listeners->fallbackLeader->fallback = nullptr;
            break;
            case XWAYLAND_POPUP:
                delete listeners->popup;
                //TODO
            break;
            default:
            break;
        }
    };
    wl_signal_add(&surface->events.dissociate, &listeners->disassociate);

    listeners->destroy.notify = [](struct wl_listener* listener, void* data) {
        XWaylandSurface* listeners = wl_container_of(listener, listeners, destroy);

        switch(listeners->type) {
            case XWAYLAND_TOPLEVEL:
                delete listeners->toplevel;
            break;
            case XWAYLAND_POPUP:
            break;
            default:
            warn("xwayland surface of unknown type");
            break;
        }

        wl_list_remove(&listeners->map.link);
        wl_list_remove(&listeners->associate.link);
        wl_list_remove(&listeners->disassociate.link);
        wl_list_remove(&listeners->destroy.link);

        auto it = surfaces.find(listeners->wlr_surface->window_id);
        surfaces.erase(it);
    };
    wl_signal_add(&surface->events.destroy, &listeners->destroy);
}

void fetch_atoms(xcb_connection_t* xc);



//XCB ATOM STUFF
//TODO: clean up this code
enum Atom {
    NET_WM_WINDOW_TYPE_NORMAL,         // A standard top-level application window.
    NET_WM_WINDOW_TYPE_DIALOG,         // A dialog window, usually modal, requiring user input.
    NET_WM_WINDOW_TYPE_UTILITY,        // A utility window, often used for small helper apps or toolboxes.
    NET_WM_WINDOW_TYPE_TOOLBAR,        // A toolbar window, typically attached to a parent application.
    NET_WM_WINDOW_TYPE_SPLASH,         // A splash screen, usually displayed during application startup.
    NET_WM_WINDOW_TYPE_MENU,           // A standalone menu window.
    NET_WM_WINDOW_TYPE_DROPDOWN_MENU,  // A dropdown menu, usually attached to a menu button.
    NET_WM_WINDOW_TYPE_POPUP_MENU,     // A popup menu, appearing on right-click or similar events.
    NET_WM_WINDOW_TYPE_TOOLTIP,        // A tooltip window, used for brief context help or information.
    NET_WM_WINDOW_TYPE_NOTIFICATION,   // A notification window, often used for alerts or system messages.
    NET_WM_WINDOW_TYPE_DESKTOP,        // A desktop background or workspace root window.
    NET_WM_WINDOW_TYPE_DOCK,           // A dock or panel, such as taskbars or status bars.
    NET_WM_WINDOW_TYPE_COMBO,          // A combo box dropdown or selection menu.
    NET_WM_WINDOW_TYPE_DND,            // A drag-and-drop icon or preview window.
    NET_WM_STATE_MODAL,                // A modal window, blocking interaction with other windows.
    ATOM_LAST
};

const char* atom_string[ATOM_LAST] = {
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_DIALOG",
    "_NET_WM_WINDOW_TYPE_UTILITY",
    "_NET_WM_WINDOW_TYPE_TOOLBAR",
    "_NET_WM_WINDOW_TYPE_SPLASH",
    "_NET_WM_WINDOW_TYPE_MENU",
    "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
    "_NET_WM_WINDOW_TYPE_POPUP_MENU",
    "_NET_WM_WINDOW_TYPE_TOOLTIP",
    "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_COMBO",
    "_NET_WM_WINDOW_TYPE_DND",
    "_NET_WM_STATE_MODAL",
};

std::map<xcb_atom_t, Atom> atoms;

xcb_atom_t WM_CLIENT_LEADER = XCB_ATOM_NONE;

wlr_xwayland* xwayland;


xcb_window_t getXcbParent(xcb_window_t window) {
    xcb_connection_t* xc = wlr_xwayland_get_xwm_connection(xwayland);
    auto cookie = xcb_get_property(xc, 0, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_ANY, 0, 2048);
    auto reply = xcb_get_property_reply(xc, cookie, 0);
    if(!reply) return 0;
    xcb_window_t* xid = (xcb_window_t*) xcb_get_property_value(reply);
    free(reply);
    if(xid) return *xid;
    return 0;
}

Surface::Parent* getParent(xcb_window_t window) {
    Surface::Parent* parent = nullptr;

    //METHOD 1: find a parent trough the parent property.
    xcb_window_t parent_window = getXcbParent(window);

    if(parent_window) {
         auto surface = getSurface(getXcbParent(window), false);

        switch(surface->type) {
            case XWAYLAND_UNASSOCIATED: break;
            case XWAYLAND_TOPLEVEL:
                parent = surface->toplevel;
                break;
            case XWAYLAND_POPUP:
                parent = surface->popup;
                break;
            default:
                warn("unhandled xwayland surfacec type!");
        }
    }

    if(parent) return parent;

    //FALLBACK METHOD: find a parent trough the leader property.
    auto leader = getLeader(window, false);
    if(leader) {
        switch(leader->type) {
            case XWAYLAND_UNASSOCIATED:
                assert(leader->fallback->toplevel);
                parent = leader->fallback->toplevel;
            break;
            case XWAYLAND_TOPLEVEL:
                parent = leader->toplevel;
                break;
            case XWAYLAND_POPUP:
                parent = leader->popup;
                break;
            default:
                warn("unhandled xwayland surfacec type!");
        }
    }
    
    if(!parent) error("no parent found for xwayland popup {}", window);
    return parent;
}

std::set<Atom> getAtoms(xcb_atom_t* atom_array, size_t size) {
    std::set<Atom> a;
    for(uint i = 0; i < size; i++) {
        auto it = atoms.find(atom_array[i]);
        a.insert(it->second);
    }
    return a;
}

XWaylandWindowType getXWaylandWindowType(wlr_xwayland_surface* surface) {
    std::set<Atom> a = getAtoms(surface->window_type, surface->window_type_len);
    //printSet(a); //debug
    if(a.find(NET_WM_WINDOW_TYPE_POPUP_MENU) != a.end()) return XWAYLAND_POPUP;
    return XWAYLAND_TOPLEVEL;
}


XWaylandSurface* getLeader(xcb_window_t window_id, bool create) {
    xcb_connection_t* xc = wlr_xwayland_get_xwm_connection(xwayland);
    if(!WM_CLIENT_LEADER) {
        error("Client Leader Atom not found");
        return nullptr;
    }
    auto cookie = xcb_get_property(xc, 0, window_id, WM_CLIENT_LEADER, XCB_ATOM_ANY, 0, 2048);
    auto reply = xcb_get_property_reply(xc, cookie, 0);
    if(!reply) {
        error("window {} has no leader", window_id);
        return nullptr;
    }
    xcb_window_t* xid = (xcb_window_t*) xcb_get_property_value(reply);
    free(reply);
    if(!xid) {
        error("window {} has no leader", window_id);
        return nullptr;
    }

    return getSurface(*xid, create);
}

void fetch_atoms(xcb_connection_t* xc) {
    for(Atom atom = (Atom) 0; atom < ATOM_LAST; atom = static_cast<Atom>(atom + 1) ) {
        const auto name = atom_string[atom];
        xcb_intern_atom_reply_t *reply;
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
        if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
            atoms[reply->atom] = atom;
        free(reply);
    }

    //client leader seperatly (needed in the other direction not the map)
    xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(xc, 0, strlen("WM_CLIENT_LEADER"), "WM_CLIENT_LEADER");
    xcb_intern_atom_reply_t *reply2 = xcb_intern_atom_reply(xc, cookie2, NULL);
    if(reply2) WM_CLIENT_LEADER = reply2->atom;
    free(reply2);
}

//TODO: xcb / xwayland selber implemente

void xwayland_ready(struct wl_listener* listener, void* data) {
    struct wlr_xcursor *xcursor;
    xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
    int err = xcb_connection_has_error(xc);
    if (err) {
        error("xcb connection error. failed with code {}",err);
        return;
    }

    fetch_atoms(xc);

    // assign the one and only seat
    wlr_xwayland_set_seat(xwayland, Input::seat);

    //Set the default XWayland cursor.
    if ((xcursor = wlr_xcursor_manager_get_xcursor(Input::cursor_mgr, "default", 1)))
        wlr_xwayland_set_cursor(xwayland,
                xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
                xcursor->images[0]->width, xcursor->images[0]->height,
                xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);

    xcb_disconnect(xc);
}

wl_listener new_xwayland_surface;
wl_listener xwayland_ready_listener;

}

void Surface::setupXWayland() {
    xwayland = wlr_xwayland_create(Server::display, Server::compositor, false);
    
    new_xwayland_surface.notify = new_xwayland_surface_notify;
    wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);
    xwayland_ready_listener.notify = xwayland_ready;
    wl_signal_add(&xwayland->events.ready, &xwayland_ready_listener);
    setenv("DISPLAY", xwayland->display_name, 1);
}