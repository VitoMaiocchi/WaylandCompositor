#include "surface.hpp"
#include "config.hpp"
#include "layout.hpp"
#include "input.hpp"
#include "output.hpp"
#include "server.hpp"

#include <wlr/util/log.h>
#include <cassert>

struct wlr_xdg_shell *xdg_shell;
struct wl_listener new_xdg_surface;
struct wl_listener new_xdg_toplevel;

bool Surface::contains(int x, int y) {
	if(extends.x > x) return false;
	if(extends.x + extends.width < x) return false;
	if(extends.y > y) return false;
	if(extends.y + extends.height < y) return false;

	return true;
}

ToplevelSurface* focused_toplevel = NULL;

ToplevelSurface::ToplevelSurface() {
	for(uint i = 0; i < 4; i++) border[i] = NULL;
	extends = {0,0,0,0};
}
 
void ToplevelSurface::setExtends(struct wlr_box extends) {
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

void ToplevelSurface::setFocused() {
	debug("SET FOCUSED BEGIN");
	ToplevelSurface* prev = focused_toplevel;

	if(this == prev) return;
	focused_toplevel = this;

	if(prev) {
		prev->setActivated(false);
		for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(prev->border[i], bordercolor_inactive);
	}

	setActivated(true);
	for(int i = 0; i < 4; i++) wlr_scene_rect_set_color(border[i], bordercolor_active);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(Input::seat);
	if (keyboard) wlr_seat_keyboard_notify_enter(Input::seat, getSurface(),
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);

	debug("SET FOCUSED END");
}

void ToplevelSurface::map_notify() {
	Layout::addSurface(this);
	setFocused();
}

void ToplevelSurface::unmap_notify() {
	debug("UMAP NOTIFY");

	//remove surface from layout
	ToplevelSurface* next = Layout::removeSurface(this);
	if(focused_toplevel	== this) {
		focused_toplevel = NULL;
		if(next) next->setFocused();
	}

	//destroy window decorations
	for(unsigned i = 0; i < 4; i++) {
		assert(border[i]);
		wlr_scene_node_destroy(&border[i]->node);
		border[i] = NULL;
	}
}

std::pair<int, int> ToplevelSurface::surfaceCoordinateTransform(int x, int y) {
	return {x - (extends.x + BORDERWIDTH), y - (extends.y + BORDERWIDTH)};
}

wlr_surface* XdgToplevelSurface::getSurface() {
	return xdg_toplevel->base->surface;
}

void XdgToplevelSurface::setActivated(bool activated) {
	wlr_xdg_toplevel_set_activated(xdg_toplevel, activated);
}

void XdgToplevelSurface::setSurfaceSize(uint width, uint height) {
	wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);
}

XdgToplevelSurface::XdgToplevelSurface(wlr_xdg_toplevel* xdg_toplevel) {
	debug("TOPLEVEL SURFACE");

	this->xdg_toplevel = xdg_toplevel;
    root_node = wlr_scene_tree_create(&Output::scene->tree);
	surface_node = wlr_scene_xdg_surface_create(root_node, xdg_toplevel->base);
	//xdg_surface->data = surface_node;   //WEISS NÖD OB DAS MIT EM REDESIGN NÖTIG ISCH
                                        //evtl isch gschider da XdgTopLevel ane mache

    	//CONFIGURE LISTENERS
    map_listener.notify = [](struct wl_listener* listener, void* data) {
		debug("MAP SURFACE");
        XdgToplevelSurface* surface = wl_container_of(listener, surface, map_listener);
        surface->map_notify();
    };

    unmap_listener.notify = [](struct wl_listener* listener, void* data) {
		debug("UMAP SURFACE");
        XdgToplevelSurface* surface = wl_container_of(listener, surface, unmap_listener);
        surface->unmap_notify();
    };

    destroy_listener.notify = [](struct wl_listener* listener, void* data) {
		debug("DESTROY SURFACE");
        XdgToplevelSurface* surface = wl_container_of(listener, surface, destroy_listener);
        delete surface;
    };

	wl_signal_add(&xdg_toplevel->base->surface->events.map, 		&map_listener);
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, 		&unmap_listener);
	wl_signal_add(&xdg_toplevel->base->surface->events.destroy, 	&destroy_listener);
}

XdgToplevelSurface::~XdgToplevelSurface() {
    wlr_scene_node_destroy(&root_node->node);

	wl_list_remove(&map_listener.link);
	wl_list_remove(&unmap_listener.link);
	wl_list_remove(&destroy_listener.link);
}


void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
	debug("NEW XDG TOPLEVEL NOTIFY");
	wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
	new XdgToplevelSurface(xdg_toplevel);
}

void setupSurface() {
	debug("SURFACE SETUP");
	xdg_shell = wlr_xdg_shell_create(Server::display, 3);
	
	new_xdg_toplevel.notify = new_xdg_toplevel_notify;
	wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);	

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
}