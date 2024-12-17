#include "surface.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include "server.hpp"

#include <wlr/util/log.h>
#include <cassert>
#include <map>
#include <iostream>
#include <set>

namespace Surface {
	//BASE CLASS
	bool Base::contains(int x, int y) {
		if(extends.x > x) return false;
		if(extends.x + extends.width < x) return false;
		if(extends.y > y) return false;
		if(extends.y + extends.height < y) return false;
		return true;
	}

	void Base::setExtends(wlr_box extends) {
		bool resize = this->extends.width != extends.width || this->extends.height != extends.height;
		this->extends = extends;
		extendsUpdateNotify(resize);
	}

	//TOPLEVEL
	Toplevel::Toplevel() {
		for(uint i = 0; i < 4; i++) border[i] = NULL;
		extends = {0,0,0,0};
		visible = false;
		focused = false;
	}
	
	void Toplevel::setFocus(bool focus) {
		//FIXME: remove keyboard wenn unfocus
		if(focus == focused) return;
		focused = focus;
		if(!visible) return;

		if(!focus) debug("UNFOCUS");
		if(focus) debug("FOCUS");
		setActivated(focus);
		if(!focus) {
			for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(border[i], bordercolor_inactive);
			return;
		}

		for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(border[i], bordercolor_active);

		Input::setKeyboardFocus(this);
	}

	void Toplevel::setVisibility(bool visible) {
		if(this->visible == visible) return;
		this->visible = visible;
		
		if(!visible) {
			wlr_scene_node_set_enabled(&root_node->node, false);
			return;
		}
		
		wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);
		setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
		wlr_scene_node_set_position(&surface_node->node, BORDERWIDTH, BORDERWIDTH);

		wlr_scene_node_set_enabled(&root_node->node, true);

		if(!border[0]) { //create window decorations if they don't allready exist
			auto color = focused? bordercolor_active : bordercolor_inactive;
			border[0] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, color);
			border[1] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, color);
			border[2] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, color);
			border[3] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, color);

			wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
			wlr_scene_node_set_position(&border[2]->node, 0, BORDERWIDTH);
			wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);
		}

		setActivated(focused);
		if(focused) Input::setKeyboardFocus(this);
	}

	std::pair<int, int> Toplevel::surfaceCoordinateTransform(int x, int y) {
		return {x - (extends.x + BORDERWIDTH), y - (extends.y + BORDERWIDTH)};
	}

	void Toplevel::mapNotify(bool mapped) {
		if(mapped) {
			Layout::addSurface(this);
			return;
		} 

		Layout::removeSurface(this);

		for(unsigned i = 0; i < 4; i++) {
			assert(border[i]);
			wlr_scene_node_destroy(&border[i]->node);
			border[i] = NULL;
		}
	}

	void Toplevel::extendsUpdateNotify(bool resize) {
		if(!visible) return;

		//UPDATE WINDOW DECORATION
		wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);
		if(!resize) return; //the rest only involves resize

		setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
		
		wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
		wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);

		wlr_scene_rect_set_size(border[0], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(border[1], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(border[2], BORDERWIDTH, extends.height-2*BORDERWIDTH);
		wlr_scene_rect_set_size(border[3], BORDERWIDTH, extends.height-2*BORDERWIDTH);
	}



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
			debug("TOPLEVEL SURFACE");
			this->xdg_toplevel = xdg_toplevel;
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

		struct wl_listener map_listener;
		struct wl_listener unmap_listener;
		struct wl_listener destroy_listener;
	};

	void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
		debug("NEW XDG TOPLEVEL NOTIFY");
		wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
		xdg_shell_surface_listeners* listeners = new xdg_shell_surface_listeners();
		listeners->surface = new XdgToplevel(xdg_toplevel);
		xdg_toplevel->base->surface->data = listeners->surface;
		wlr_log(WLR_DEBUG, "new xdg toplevel %p", listeners->surface);

				//CONFIGURE LISTENERS
		listeners->map_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("MAP SURFACE");
			xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, map_listener);
			listeners->surface->map_event(true);
		};

		listeners->unmap_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("UMAP SURFACE");
			xdg_shell_surface_listeners* listeners = wl_container_of(listener, listeners, unmap_listener);
			listeners->surface->map_event(false);
		};

		listeners->destroy_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("DESTROY SURFACE");
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
	}

	void new_xdg_popup_notify(struct wl_listener* listener, void* data) {
		wlr_xdg_popup* xdg_popup = (wlr_xdg_popup*) data;
		if(!xdg_popup->parent) {
			wlr_log(WLR_DEBUG, "XDG toplevel new popup: no parent");
			return;
		}
		Base* surface = (Base*) xdg_popup->parent->data;
		wlr_log(WLR_DEBUG, "XDG toplevel new popup with parent: %p", surface);
		//TODO create xdg popup
	}

	void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
		struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
		wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	//XWAYLAND
	class XwaylandToplevel : public Toplevel {
		wlr_xwayland_surface* xwayland_surface;

        public:
        XwaylandToplevel(wlr_xwayland_surface* xwayland_surface) {
			this->xwayland_surface = xwayland_surface;
			root_node = wlr_scene_tree_create(&Output::scene->tree);
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
			wlr_xwayland_surface_configure(xwayland_surface, 0,0, width, height);
		}

        void setActivated(bool activated) {
			wlr_xwayland_surface_activate(xwayland_surface, activated);
		}

        wlr_surface* getSurface() {
			return xwayland_surface->surface;
		}

    };

	wlr_xwayland* xwayland;
	wl_listener new_xwayland_surface;
	wl_listener xwayland_ready_listener;

	enum XWaylandWindowType {
		XWAYLAND_UNASSOCIATED,
		XWAYLAND_TOPLEVEL,
		XWAYLAND_POPUP,
	};

	XWaylandWindowType getXWaylandWindowType(wlr_xwayland_surface* surface);

	struct xwayland_surface_listeners {
		union {
			XwaylandToplevel* surface;
		};
		wl_listener map;
		wl_listener associate;
		wl_listener disassociate;
		wl_listener destroy;

		bool mapped = false;

		wlr_xwayland_surface* wlr_surface;
		XWaylandWindowType type = XWAYLAND_UNASSOCIATED;

		void tryMap() {
			assert(type == XWAYLAND_TOPLEVEL);
			if(!mapped) return;
			surface->map(true);
		}
	};

	void new_xwayland_surface_notify(struct wl_listener* listener, void* data) {
		debug("NEW XWAYLAND SURFACE NOTIFY");
		wlr_xwayland_surface* surface = (wlr_xwayland_surface*) data;
		auto listeners = new xwayland_surface_listeners;
		listeners->wlr_surface = surface;

		listeners->associate.notify = [](struct wl_listener* listener, void* data) {
			xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, associate);
			listeners->type = getXWaylandWindowType(listeners->wlr_surface);

			wlr_log(WLR_DEBUG, "surface %p is now associated", listeners->surface);
			switch(listeners->type) {
				case XWAYLAND_TOPLEVEL:
					debug("new xwayland toplevel");
					listeners->surface = new XwaylandToplevel(listeners->wlr_surface);
					listeners->tryMap();
				break;
				case XWAYLAND_POPUP:
					debug("new xwayland popup");
					//TODO
				break;
				default:
				debug("xwayland surface of unknown type");
				break;
			}
		};
		wl_signal_add(&surface->events.associate, &listeners->associate);

		listeners->map.notify = [](struct wl_listener* listener, void* data) {
			xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, map);
			wlr_log(WLR_DEBUG, "surface %p wants to be mapped", listeners->surface);
			listeners->mapped = true;

			switch(listeners->type) {
				case XWAYLAND_TOPLEVEL:
					debug("map xwayland toplevel");
					listeners->tryMap();
				break;
				default:
				debug("unhandled map case");
				break;
			}
		};
		wl_signal_add(&surface->events.map_request, &listeners->map);

		listeners->disassociate.notify = [](struct wl_listener* listener, void* data) {
			xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, disassociate);
			wlr_log(WLR_DEBUG, "surface %p disassociated", listeners->surface);

			switch(listeners->type) {
				case XWAYLAND_TOPLEVEL:
					debug("disassociate xwayland toplevel");
					listeners->surface->map(false);
				break;
				case XWAYLAND_POPUP:
					debug("disassociate xwayland popup");
					//TODO
				break;
				default:
				debug("xwayland surface of unknown type");
				break;
			}
		};
		wl_signal_add(&surface->events.dissociate, &listeners->disassociate);

		listeners->destroy.notify = [](struct wl_listener* listener, void* data) {
			xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, destroy);
			wlr_log(WLR_DEBUG, "surface %p destroyed", listeners->surface);

			switch(listeners->type) {
				case XWAYLAND_TOPLEVEL:
					debug("destroy xwayland toplevel");
					delete listeners->surface;
				break;
				case XWAYLAND_POPUP:
					debug("destroy xwayland popup");
					//TODO
				break;
				default:
				debug("xwayland surface of unknown type");
				break;
			}

			wl_list_remove(&listeners->map.link);
			wl_list_remove(&listeners->associate.link);
			wl_list_remove(&listeners->disassociate.link);
			wl_list_remove(&listeners->destroy.link);

			delete listeners;
		};
		wl_signal_add(&surface->events.destroy, &listeners->destroy);
	}

	void fetch_atoms(xcb_connection_t* xc);

	void xwayland_ready(struct wl_listener* listener, void* data) {
		debug("X WAYLAND READY");
		struct wlr_xcursor *xcursor;
		xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
		int err = xcb_connection_has_error(xc);
		if (err) {
			fprintf(stderr, "xcb_connect to X server failed with code %d\n. Continuing with degraded functionality.\n", err);
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

	void setup() {
		debug("SURFACE SETUP");

		xdg_shell = wlr_xdg_shell_create(Server::display, 3);
		new_xdg_toplevel.notify = new_xdg_toplevel_notify;
		new_xdg_popup.notify = new_xdg_popup_notify;
		wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);	
		wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

		xwayland = wlr_xwayland_create(Server::display, Server::compositor, false);
		
		new_xwayland_surface.notify = new_xwayland_surface_notify;
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);
		xwayland_ready_listener.notify = xwayland_ready;
		wl_signal_add(&xwayland->events.ready, &xwayland_ready_listener);
		setenv("DISPLAY", xwayland->display_name, 1);

			//requesting serverside decorations of clients
		wlr_server_decoration_manager_set_default_mode(
				wlr_server_decoration_manager_create(Server::display),
				WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
		xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(Server::display);
		xdg_decoration_listener.notify = xdg_new_decoration_notify;
		wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration, &xdg_decoration_listener);
	}


	//XCB ATOM STUFF
	//TODO: clean up this code
	enum Atom {
		NET_WM_WINDOW_TYPE_NORMAL,
		NET_WM_WINDOW_TYPE_DIALOG,
		NET_WM_WINDOW_TYPE_UTILITY,
		NET_WM_WINDOW_TYPE_TOOLBAR,
		NET_WM_WINDOW_TYPE_SPLASH,
		NET_WM_WINDOW_TYPE_MENU,
		NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
		NET_WM_WINDOW_TYPE_POPUP_MENU,
		NET_WM_WINDOW_TYPE_TOOLTIP,
		NET_WM_WINDOW_TYPE_NOTIFICATION,
		NET_WM_STATE_MODAL,
		ATOM_LAST,
	};

	const char* atom_string[ATOM_LAST] = {
		[NET_WM_WINDOW_TYPE_NORMAL] 		= "_NET_WM_WINDOW_TYPE_NORMAL",
		[NET_WM_WINDOW_TYPE_DIALOG] 		= "_NET_WM_WINDOW_TYPE_DIALOG",
		[NET_WM_WINDOW_TYPE_UTILITY] 		= "_NET_WM_WINDOW_TYPE_UTILITY",
		[NET_WM_WINDOW_TYPE_TOOLBAR] 		= "_NET_WM_WINDOW_TYPE_TOOLBAR",
		[NET_WM_WINDOW_TYPE_SPLASH] 		= "_NET_WM_WINDOW_TYPE_SPLASH",
		[NET_WM_WINDOW_TYPE_MENU] 			= "_NET_WM_WINDOW_TYPE_MENU",
		[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] 	= "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
		[NET_WM_WINDOW_TYPE_POPUP_MENU] 	= "_NET_WM_WINDOW_TYPE_POPUP_MENU",
		[NET_WM_WINDOW_TYPE_TOOLTIP] 		= "_NET_WM_WINDOW_TYPE_TOOLTIP",
		[NET_WM_WINDOW_TYPE_NOTIFICATION] 	= "_NET_WM_WINDOW_TYPE_NOTIFICATION",
		[NET_WM_STATE_MODAL] 				= "_NET_WM_STATE_MODAL",
	};

	/*
	die gis au no 
	"_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_WINDOW_TYPE_COMBO",
    "_NET_WM_WINDOW_TYPE_DND",
	*/


	std::map<xcb_atom_t, Atom> atoms;

	std::set<Atom> getAtoms(xcb_atom_t* atom_array, size_t size) {
		std::set<Atom> a;
		for(uint i = 0; i < size; i++) {
			auto it = atoms.find(atom_array[i]);
			a.insert(it->second);
		}
		return a;
	}

	void printSet(std::set<Atom> atom) {
		std::cout << "Atoms len:" << atom.size() << std::endl;
		for(auto a : atom) std::cout << atom_string[a] << std::endl;
	}


	XWaylandWindowType getXWaylandWindowType(wlr_xwayland_surface* surface) {
		std::set<Atom> a = getAtoms(surface->window_type, surface->window_type_len);
		printSet(a); //debug
		if(a.find(NET_WM_WINDOW_TYPE_POPUP_MENU) != a.end()) return XWAYLAND_POPUP;
		return XWAYLAND_TOPLEVEL;
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
	}

}
