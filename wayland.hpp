#ifndef VITOWM_WAYLAND_H
#define VITOWM_WAYLAND_H

//void client_set_extends(struct LayoutNode* client, const struct wlr_box extends);

#include "includes.hpp"
#include "surface.hpp"

struct WaylandServer {
    struct wl_display* display;
	struct wlr_backend* backend;
	struct wlr_renderer* renderer;
	struct wlr_allocator* allocator;
	struct wlr_scene* scene;
	struct wlr_scene_output_layout* scene_layout;

	struct wlr_output_layout* output_layout;
	struct wl_list outputs;
	struct wl_listener new_output_listener;

	struct wlr_cursor* cursor;
	struct wlr_xcursor_manager* cursor_mgr;
	ToplevelSurface* pointer_focus;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat* seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_listener new_xdg_toplevel;
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
    struct wl_listener xdg_decoration_listener;

	struct wlr_layer_shell_v1* layer_shell;
	struct wl_listener new_layer_shell_surface;
	struct wl_listener delete_layer_shell_surface;

	struct WaylandClient* focusedClient;
};

extern WaylandServer server;

#endif