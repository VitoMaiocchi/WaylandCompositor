#include "surface.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include "server.hpp"

#include <wlr/util/log.h>
#include <cassert>

namespace Surface {
	struct wlr_xdg_shell *xdg_shell;
	//struct wl_listener new_xdg_surface;
	struct wl_listener new_xdg_toplevel;

	wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	wl_listener xdg_decoration_listener;

	bool Base::contains(int x, int y) {
		if(extends.x > x) return false;
		if(extends.x + extends.width < x) return false;
		if(extends.y > y) return false;
		if(extends.y + extends.height < y) return false;

		return true;
	}

	Toplevel::Toplevel() {
		for(uint i = 0; i < 4; i++) border[i] = NULL;
		extends = {0,0,0,0};
	}
	
	void Toplevel::setExtends(struct wlr_box extends) {
		struct wlr_box oldext = this->extends;
		this->extends = extends;

		//TODO: DA NUR WENN ES GMAPPED UND VISIBLE ISCH WITERMACHE
		//ODER EVTL SÖTT NUR CALLED WERDE WENN VISIBLE KA
		//uf jede fall wird so vill errors ge
		//das isch jetz au scho wider outdated aber ich lan de kommentar zur sicherheit no da

		//TODO: REMOVE CODE DUPLICATION

		if(!border[0]) {
			//CREATE WINDOW DECORATION
			wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);

			setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
			wlr_scene_node_set_position(&surface_node->node, BORDERWIDTH, BORDERWIDTH);
			
			border[0] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, bordercolor_inactive);
			border[1] = wlr_scene_rect_create(root_node, extends.width, BORDERWIDTH, bordercolor_inactive);
			border[2] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, bordercolor_inactive);
			border[3] = wlr_scene_rect_create(root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, bordercolor_inactive);

			wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
			wlr_scene_node_set_position(&border[2]->node, 0, BORDERWIDTH);
			wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);
			return;
		}

		//UPDATE WINDOW DECORATION
		wlr_scene_node_set_position(&root_node->node, extends.x, extends.y);

		bool resize = oldext.width != extends.width || oldext.height != extends.height;
		if(!resize) return; //the rest only involves resize

		setSurfaceSize(extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
		
		wlr_scene_node_set_position(&border[1]->node, 0, extends.height-BORDERWIDTH);
		wlr_scene_node_set_position(&border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);

		wlr_scene_rect_set_size(border[0], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(border[1], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(border[2], BORDERWIDTH, extends.height-2*BORDERWIDTH);
		wlr_scene_rect_set_size(border[3], BORDERWIDTH, extends.height-2*BORDERWIDTH);
	}

	void Toplevel::setFocus(bool focus) {
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

	void Toplevel::map_notify() {
		Layout::addSurface(this);
	}

	void Toplevel::unmap_notify() {
		debug("UMAP NOTIFY");

		//remove surface from layout
		Layout::removeSurface(this);

		//destroy window decorations
		for(unsigned i = 0; i < 4; i++) {
			assert(border[i]);
			wlr_scene_node_destroy(&border[i]->node);
			border[i] = NULL;
		}
	}

	std::pair<int, int> Toplevel::surfaceCoordinateTransform(int x, int y) {
		return {x - (extends.x + BORDERWIDTH), y - (extends.y + BORDERWIDTH)};
	}

	wlr_surface* XdgToplevel::getSurface() {
		return xdg_toplevel->base->surface;
	}

	void XdgToplevel::setActivated(bool activated) {
		wlr_xdg_toplevel_set_activated(xdg_toplevel, activated);
	}

	void XdgToplevel::setSurfaceSize(uint width, uint height) {
		wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);
	}

	XdgToplevel::XdgToplevel(wlr_xdg_toplevel* xdg_toplevel) {
		debug("TOPLEVEL SURFACE");

		this->xdg_toplevel = xdg_toplevel;
		root_node = wlr_scene_tree_create(&Output::scene->tree);
		surface_node = wlr_scene_xdg_surface_create(root_node, xdg_toplevel->base);
		//xdg_surface->data = surface_node;   //WEISS NÖD OB DAS MIT EM REDESIGN NÖTIG ISCH
											//evtl isch gschider da XdgTopLevel ane mache

	}

	Toplevel::~Toplevel() {}

	XdgToplevel::~XdgToplevel() {
		wlr_scene_node_destroy(&root_node->node);
	}

	struct TopLevelListeners {
		Toplevel* toplevel;

		struct wl_listener map_listener;
		struct wl_listener unmap_listener;
		struct wl_listener destroy_listener;
	};

	void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
		debug("NEW XDG TOPLEVEL NOTIFY");
		wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
		TopLevelListeners* listeners = new TopLevelListeners();
		listeners->toplevel = new XdgToplevel(xdg_toplevel);

				//CONFIGURE LISTENERS
		listeners->map_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("MAP SURFACE");
			TopLevelListeners* listeners = wl_container_of(listener, listeners, map_listener);
			listeners->toplevel->map_notify();
		};

		listeners->unmap_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("UMAP SURFACE");
			TopLevelListeners* listeners = wl_container_of(listener, listeners, unmap_listener);
			listeners->toplevel->unmap_notify();
		};

		listeners->destroy_listener.notify = [](struct wl_listener* listener, void* data) {
			debug("DESTROY SURFACE");
			TopLevelListeners* listeners = wl_container_of(listener, listeners, destroy_listener);
			delete listeners->toplevel;

			wl_list_remove(&listeners->map_listener.link);
			wl_list_remove(&listeners->unmap_listener.link);
			wl_list_remove(&listeners->destroy_listener.link);

			delete listeners;
		};

		wl_signal_add(&xdg_toplevel->base->surface->events.map, 		&listeners->map_listener);
		wl_signal_add(&xdg_toplevel->base->surface->events.unmap, 		&listeners->unmap_listener);
		wl_signal_add(&xdg_toplevel->base->surface->events.destroy, 	&listeners->destroy_listener);
	}

	void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
		struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
		wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	//XWAYLAND ULTRA (no ultra scheisse)
	wlr_xwayland* xwayland;
	wl_listener new_xwayland_surface;
	wl_listener xwayland_ready_listener;

	struct xwayland_surface_listeners {
		wlr_xwayland_surface* surface;
		wl_listener map;
		wl_listener associate;
	};

	void new_xwayland_surface_notify(struct wl_listener* listener, void* data) {
		debug("NEW XWAYLAND SURFACE NOTIFY");
		struct wlr_xwayland_surface* surface = (wlr_xwayland_surface*) data;
		auto listeners = new xwayland_surface_listeners;
		listeners->surface = surface;

		listeners->associate.notify = [](struct wl_listener* listener, void* data) {
			xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, associate);
			debug("YAY IM ASSOCIATED I AM THE XWAYLAND WINDOW");

			wlr_scene_tree* parent = wlr_scene_tree_create(&Output::scene->tree);
				assert(parent);
				assert(listeners);
				assert(listeners->surface);
				assert(listeners->surface->surface);
			wlr_scene_subsurface_tree_create(parent, listeners->surface->surface);
			wlr_xwayland_surface_configure(listeners->surface, 0, 0, 800, 600);

			//das passiert irgendwie nöd
			listeners->map.notify = [](struct wl_listener* listener, void* data) {
				xwayland_surface_listeners* listeners = wl_container_of(listener, listeners, map);
				//listeners->surface
				//wlr_xwayland_surface_configure()
				debug("GOOGGOO GAGA I WANT TO BE MAPPED");
				wlr_scene_tree* parent = wlr_scene_tree_create(&Output::scene->tree);
				debug("GOOGGOO GAGA I WANT TO BE MAPPED 1");
				assert(parent);
				assert(listeners);
				assert(listeners->surface);
				assert(listeners->surface->surface);
				wlr_scene_subsurface_tree_create(parent, listeners->surface->surface);
				debug("GOOGGOO GAGA I WANT TO BE MAPPED 3");
				//wlr_xwayland_surface_configure(listeners->surface, 0, 0, 800, 600);
				//debug("GOOGGOO GAGA I WANT TO BE MAPPED 4");
			};
			wl_signal_add(&listeners->surface->events.map_request, &listeners->map);

		};
		wl_signal_add(&surface->events.associate, &listeners->associate);
	}

	//kein plan was das macht
	enum { NetWMWindowTypeDialog, NetWMWindowTypeSplash, NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility, NetLast }; /* EWMH atoms */
	static xcb_atom_t netatom[NetLast];
	xcb_atom_t getatom(xcb_connection_t *xc, const char *name) {
		xcb_atom_t atom = 0;
		xcb_intern_atom_reply_t *reply;
		xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
		if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
			atom = reply->atom;
		free(reply);

		return atom;
	}

	void xwayland_ready(struct wl_listener* listener, void* data) {
		debug("X WAYLAND READY");
		struct wlr_xcursor *xcursor;
		xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
		int err = xcb_connection_has_error(xc);
		if (err) {
			fprintf(stderr, "xcb_connect to X server failed with code %d\n. Continuing with degraded functionality.\n", err);
			return;
		}

		/* Collect atoms we are interested in. If getatom returns 0, we will
		* not detect that window type. */
		netatom[NetWMWindowTypeDialog] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
		netatom[NetWMWindowTypeSplash] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
		netatom[NetWMWindowTypeToolbar] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
		netatom[NetWMWindowTypeUtility] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

		/* assign the one and only seat */
		wlr_xwayland_set_seat(xwayland, Input::seat);

		/* Set the default XWayland cursor to match the rest of dwl. */
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
		wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);	

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

		//TODO: DA MÜND NO POPUPS GHANDLED WERDE
		/*
		if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
			//parent data = the wlr_scene_tree of parent
			struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
			assert(parent != NULL);
			xdg_surface->data = wlr_scene_xdg_surface_create(parent->data, xdg_surface);
			return;
		}
		*/

		//TODO: LAYER SHELL IMPLEMENT das isch für so rofi und so
		/*
		server.layer_shell = wlr_layer_shell_v1_create(server.display, 4);
		server.new_layer_shell_surface.notify = layer_shell_new_surface;
		server.delete_layer_shell_surface.notify = layer_shell_delete_surface;
		wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_shell_surface);
		//FIXME: DELTE isch glaub wenn LAYERSHELL DELTED WIRD nöd die SURFACE
		wl_signal_add(&server.layer_shell->events.destroy, &server.delete_layer_shell_surface);
		wl_list_init(&lss_list);
		//server.layer_shell->events.new_surface
		*/
	}


	//LAYER SHELL ULTRA BASIC
	/*
	struct LayerShellSurface {
		struct wl_list link;
		struct wlr_layer_surface_v1* ls_surface; 
		struct wlr_scene_layer_surface_v1* surface_node;
	};

	struct wl_list lss_list;

	void layer_shell_new_surface(struct wl_listener *listener, void *data) {
		struct wlr_layer_surface_v1* surface = (wlr_layer_surface_v1*) data;
		if(!surface) return;

		struct LayerShellSurface* lss = (LayerShellSurface* ) calloc(1, sizeof(struct LayerShellSurface));
		wl_list_insert(&lss_list, &lss->link);
		lss->ls_surface = surface;

		struct wlr_output* output = surface->output;
		if(!output) {
			if (wl_list_empty(&server.outputs)) return;
			struct WaylandOutput* wlo = wl_container_of(server.outputs.next, wlo, link);
			output = wlo->wlr_output;
		}

		lss->surface_node = wlr_scene_layer_surface_v1_create(&server.scene->tree, surface);
		//FIXME: ULTRA JANKY (0,0) ich muss für das zerscht d outputs besser mache
		wlr_scene_node_set_position(&lss->surface_node->tree->node, 0, 0);
		wlr_layer_surface_v1_configure(surface, output->width, output->height);

		//TODO: FREE LAYERSHELL eventually -> memory leak
	}

	void layer_shell_delete_surface(struct wl_listener *listener, void *data) {
		//idk ob ich da was will
	}
	*/
}
