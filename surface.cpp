#include "surface.h"
#include "config.h"
//extern "C" {
#include "wayland.hpp"
//}

#include <wlr/util/log.h>
#include <cassert>

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


void XdgToplevelSurface::setSurfaceSize(uint width, uint height) {
	wlr_xdg_toplevel_set_size(xdg_toplevel, width, height);
}

XdgToplevelSurface::XdgToplevelSurface(wlr_xdg_toplevel* xdg_toplevel) {

	debug("TOPLEVEL SURFACE");

	this->xdg_toplevel = xdg_toplevel;
    root_node = wlr_scene_tree_create(&server.scene->tree);
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
    //wlr_log(WLR_DEBUG, "destroy surface");
    wlr_scene_node_destroy(&root_node->node);

	wl_list_remove(&map_listener.link);
	wl_list_remove(&unmap_listener.link);
	wl_list_remove(&destroy_listener.link);
}

void ToplevelSurface::map_notify() {
    struct wlr_box extends = {100, 100, 1000, 800};
    this->extends = extends;
	setExtends(extends);
    //window_decoration_update() private und so nöd richtig
    //theoretisch set focused
}

void ToplevelSurface::unmap_notify() {
	//DESTROY WINDOW DECORATION
	for(unsigned i = 0; i < 4; i++) {
		assert(border[i]);
		wlr_scene_node_destroy(&border[i]->node);
		border[i] = NULL;
	}
}



void new_xdg_toplevel_notify(struct wl_listener* listener, void* data) {
	debug("NEW XDG TOPLEVEL NOTIFY");
	wlr_xdg_toplevel* xdg_toplevel = (wlr_xdg_toplevel*) data;
	new XdgToplevelSurface(xdg_toplevel);
}


void setupSurface();


void setupSurface() {



	//assert( (void* )&(server->display) != (void*) server->display);
	wlr_log(WLR_DEBUG, "server 2: %p", &server);
	wlr_log(WLR_DEBUG, "aaah place: %p", &server.display );
	wlr_log(WLR_DEBUG, "aaah value: %p", server.display);
	debug("SURFACE SETUP");
	server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
	
	//server.new_xdg_surface.notify = xdg_shell_new_surface_notify;
	//wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);
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
	debug("1");
	server.new_xdg_toplevel.notify = new_xdg_toplevel_notify;
	debug("2");
	wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	debug("SURFACE SETUP FERTIG");
	
}