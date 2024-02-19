#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "layout.h"

#define BORDERWIDTH 2

#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

const float bordercolor[] = COLOR(0xFF0000FF);

static struct WaylandServer {
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
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
    struct wl_listener xdg_decoration_listener;

	struct wlr_layer_shell_v1* layer_shell;
} server;

struct WaylandClient {
	struct wlr_xdg_toplevel *xdg_toplevel;

	//struct wlr_box extends;
	struct wlr_scene_tree* root_node;
	struct wlr_scene_tree* surface_node;
	struct wlr_scene_rect* border[4]; // top, bottom, left, right

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;

	struct LayoutNode layoutClient;
};

struct WaylandKeyboard {
	struct wl_list link;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

struct WaylandOutput {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;

	struct wlr_box extends;
};


    //CLIENTS AND SURFACES

void client_set_focused(struct LayoutNode* client) {
	if(!client) return;
	struct WaylandClient* wlClient = wl_container_of(client, wlClient, layoutClient);

	struct wlr_surface* surface = wlClient->xdg_toplevel->base->surface;
	struct wlr_surface* prev_surface = server.seat->keyboard_state.focused_surface;
	if(prev_surface == surface) return;

	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		//ich glaub das isch mit server side decorations nöd nötig. (unde au nöd) TODO: bordercolor
		if (prev_toplevel != NULL) wlr_xdg_toplevel_set_activated(prev_toplevel, false);
	}

	wlr_xdg_toplevel_set_activated(wlClient->xdg_toplevel, true);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server.seat);
	if (keyboard) wlr_seat_keyboard_notify_enter(server.seat, wlClient->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
}

void client_set_extends(struct LayoutNode* client, const struct wlr_box extends) {
	if(!client) return;
	struct WaylandClient* wlClient = wl_container_of(client, wlClient, layoutClient);

	wlr_scene_node_set_position(&wlClient->root_node->node, extends.x, extends.y);

	wlr_xdg_toplevel_set_size(wlClient->xdg_toplevel, extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
	wlr_scene_node_set_position(&wlClient->surface_node->node, BORDERWIDTH, BORDERWIDTH);
	
	if(!wlClient->border[0]) {
		wlClient->border[0] = wlr_scene_rect_create(wlClient->root_node, extends.width, BORDERWIDTH, bordercolor);
		wlClient->border[1] = wlr_scene_rect_create(wlClient->root_node, extends.width, BORDERWIDTH, bordercolor);
		wlClient->border[2] = wlr_scene_rect_create(wlClient->root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, bordercolor);
		wlClient->border[3] = wlr_scene_rect_create(wlClient->root_node, BORDERWIDTH, extends.height-2*BORDERWIDTH, bordercolor);
	} else {
		wlr_scene_rect_set_size(wlClient->border[0], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(wlClient->border[1], extends.width, BORDERWIDTH);
		wlr_scene_rect_set_size(wlClient->border[2], BORDERWIDTH, extends.height-2*BORDERWIDTH);
		wlr_scene_rect_set_size(wlClient->border[3], BORDERWIDTH, extends.height-2*BORDERWIDTH);
	}
	wlr_scene_node_set_position(&wlClient->border[1]->node, 0, extends.height-BORDERWIDTH);
	wlr_scene_node_set_position(&wlClient->border[2]->node, 0, BORDERWIDTH);
	wlr_scene_node_set_position(&wlClient->border[3]->node, extends.width-BORDERWIDTH, BORDERWIDTH);

}

static void client_xdg_surface_map_notify(struct wl_listener* listener, void* data) {
	struct WaylandClient* client = wl_container_of(listener, client, map);

	//struct wlr_box extends = {100, 100, 1000, 800};
	//client_set_extends(&client->layoutClient, extends);
	//client->xdg_toplevel->requested.
	if(client->xdg_toplevel->parent == client->xdg_toplevel) wlr_log(WLR_INFO, "goo goo gaga");
	if(client->xdg_toplevel->base->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL)
		layout_add_client(&client->layoutClient);

	client_set_focused(&client->layoutClient);
}

static void client_xdg_surface_unmap_notify(struct wl_listener* listener, void* data) {
	struct WaylandClient* client = wl_container_of(listener, client, unmap);
	for(unsigned i = 0; i < 4; i++) {
		wlr_scene_node_destroy(&client->border[i]->node);
		client->border[i] = NULL;
	}
	layout_remove_client(&client->layoutClient);
}

static void client_xdg_surface_destroy_notify(struct wl_listener* listener, void* data) {
	wlr_log(WLR_DEBUG, "destroy surface");
	struct WaylandClient* client = wl_container_of(listener, client, destroy);

	wlr_scene_node_destroy(&client->root_node->node);

	wl_list_remove(&client->map.link);
	wl_list_remove(&client->unmap.link);
	wl_list_remove(&client->destroy.link);

	free(client);
}

static void xdg_shell_new_surface_notify(struct wl_listener* listener, void* data) {
	struct wlr_xdg_surface *xdg_surface = data;

	//HANDLE POPUP CASE
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		//parent data = the wlr_scene_tree of parent
		struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);
		assert(parent != NULL);
		xdg_surface->data = wlr_scene_xdg_surface_create(parent->data, xdg_surface);
		return;
	}

	//CREATE CLIENT FOR SURFACE
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	struct WaylandClient* client = calloc(1, sizeof(*client));
	client->xdg_toplevel = xdg_surface->toplevel;
	client->root_node = wlr_scene_tree_create(&server.scene->tree);
	client->surface_node = wlr_scene_xdg_surface_create(client->root_node, xdg_surface);
	xdg_surface->data = client->surface_node;


	//CONFIGURE LISTENERS
	client->map.notify 		= client_xdg_surface_map_notify;
	client->unmap.notify 	= client_xdg_surface_unmap_notify;
	client->destroy.notify 	= client_xdg_surface_destroy_notify;
	wl_signal_add(&xdg_surface->surface->events.map, 		&client->map);
	wl_signal_add(&xdg_surface->surface->events.unmap, 		&client->unmap);
	wl_signal_add(&xdg_surface->surface->events.destroy, 	&client->destroy);
}


void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *dec = data;
	wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

    // OUTPUTS (MONITORS)

void output_frame_notify(struct wl_listener *listener, void *data) {
    //render frame
	struct WaylandOutput *output = wl_container_of(listener, output, frame);

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(server.scene, output->wlr_output);
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void output_request_state_notify(struct wl_listener *listener, void *data) {
    //output state (resolution etc) changes
    struct WaylandOutput *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);

	output->extends.width = output->wlr_output->width;
	output->extends.height = output->wlr_output->height;
	layout_init(output->extends);
}

void output_destroy_notify(struct wl_listener *listener, void *data) {
	struct WaylandOutput *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

void backend_new_output_notify(struct wl_listener *listener, void *data) {
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server.allocator, server.renderer);

    //configure output state: ultra basic da chan den so advancte shit ane.
	struct wlr_output_state state;
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	if (mode) wlr_output_state_set_mode(&state, mode);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	//create struct
	struct WaylandOutput *output = calloc(1, sizeof(*output));
	output->wlr_output = wlr_output;
	wl_list_insert(&server.outputs, &output->link);

    //Listeners
	output->frame.notify = output_frame_notify;
	output->request_state.notify = output_request_state_notify;
	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);


    //basic output configuration. das chani den no advanced mache.
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server.output_layout, wlr_output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server.scene, wlr_output);
	wlr_scene_output_layout_add_output(server.scene_layout, l_output, scene_output);

	output->extends.width = wlr_output->width;
	output->extends.height = wlr_output->height;
	output->extends.x = scene_output->x;
	output->extends.y = scene_output->y;
	layout_init(output->extends);
}


// INPUT HANDLING

void cursor_motion_notify(struct wl_listener *listener, void *data) {
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server.cursor, &event->pointer->base, event->delta_x, event->delta_y);
	wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
}

void cursor_motion_absolute_notify(struct wl_listener *listener, void *data) {
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);
	wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
}

void cursor_button_notify(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;
	wlr_seat_pointer_notify_button(server.seat, event->time_msec, event->button, event->state);
}

void cursor_axis_notify(struct wl_listener *listener, void *data) {
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server.seat, event->time_msec, event->orientation, 
            event->delta, event->delta_discrete, event->source);
}

void cursor_frame_notify(struct wl_listener *listener, void *data) {
	wlr_seat_pointer_notify_frame(server.seat);
}

void seat_request_set_cursor_notify(struct wl_listener *listener, void *data) {
    //client requests set cursor image
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = server.seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(server.cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

void seat_request_set_selection_notify(struct wl_listener *listener, void *data) {
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server.seat, event->source, event->serial);
}

void add_new_pointer_device(struct wlr_input_device* device) {
	wlr_cursor_attach_input_device(server.cursor, device);
}

static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct WaylandKeyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->wlr_keyboard->modifiers);
}

//return true if the keybind exists
static bool handle_keybinding(xkb_keysym_t sym, uint32_t modmask) {

	//printf("KEY PRESSED: %i", sym);
	wlr_log(WLR_INFO, "KEY PRESSED = %i", sym);
	if(!(modmask & WLR_MODIFIER_LOGO)) return false;

	wlr_log(WLR_INFO, "WITH WIN KEY PRESSED = %i", sym);

	if((modmask & WLR_MODIFIER_SHIFT) && sym == XKB_KEY_Q) {
		wl_display_terminate(server.display);
		return true;
	}

	switch (sym) {
	case XKB_KEY_p:
	case XKB_KEY_space:
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", "wofi --show drun", (void *)NULL);
			return 0;
		}
		break;
	case XKB_KEY_o:
	case XKB_KEY_KP_Enter:
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", "foot", (void *)NULL);
			return 0;
		}
		break;
	default:
		return false;
	}
	return true;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct WaylandKeyboard *keyboard = wl_container_of(listener, keyboard, key);
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server.seat;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modmask = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(syms[i], modmask);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	/* This event is raised by the keyboard base wlr_input_device to signal
	 * the destruction of the wlr_keyboard. It will no longer receive events
	 * and should be destroyed.
	 */
	struct WaylandKeyboard *keyboard = wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

void add_new_keyboard_device(struct wlr_input_device* device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct WaylandKeyboard *keyboard = calloc(1, sizeof(*keyboard));
	keyboard->wlr_keyboard = wlr_keyboard;

	struct xkb_rule_names layout_de = {"", "", "de", "", ""};
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &layout_de,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	//setup event listeners
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	keyboard->key.notify = keyboard_handle_key;
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	wl_signal_add(&device->events.destroy, &keyboard->destroy);


	wlr_seat_set_keyboard(server.seat, keyboard->wlr_keyboard);
	wl_list_insert(&server.keyboards, &keyboard->link);
}

void backend_new_input_notify(struct wl_listener *listener, void *data) {
    struct wlr_input_device *device = data;

	switch (device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			add_new_keyboard_device(device);
		break;
		case WLR_INPUT_DEVICE_POINTER:
			add_new_pointer_device(device);
		break;
		default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server.keyboards)) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(server.seat, caps);
}



	//SETUP

#define ASSERT_NOTNULL(n, m) if(!n) {   \
    wlr_log(WLR_ERROR, m);              \
    return 1;                           \
}

bool waylandSetup() {
    wlr_log_init(WLR_DEBUG, NULL);

    server.display = wl_display_create();

	server.backend = wlr_backend_autocreate(server.display, NULL);
    ASSERT_NOTNULL(server.backend, "failed to create wlr_backend");

	server.renderer = wlr_renderer_autocreate(server.backend);
    ASSERT_NOTNULL(server.renderer, "failed to create wlr_renderer");
	wlr_renderer_init_wl_display(server.renderer, server.display);

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    ASSERT_NOTNULL(server.allocator, "failed to create wlr_allocator");

	wlr_compositor_create(server.display, 5, server.renderer);
	wlr_subcompositor_create(server.display);
	wlr_data_device_manager_create(server.display);

	server.output_layout = wlr_output_layout_create();
	wl_list_init(&server.outputs);
	server.new_output_listener.notify = backend_new_output_notify;
	wl_signal_add(&server.backend->events.new_output, &server.new_output_listener);

	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

    //xdg shell for handling new surfaces
	server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
	server.new_xdg_surface.notify = xdg_shell_new_surface_notify;
	wl_signal_add(&server.xdg_shell->events.new_surface, &server.new_xdg_surface);

	//TODO: LAYER SHELL IMPLEMENT das isch für so rofi und so
	//server.layer_shell = wlr_layer_shell_v1_create(server.display, 4);
	//server.layer_shell->events.

    //requesting serverside decorations of clients
	wlr_server_decoration_manager_set_default_mode(
			wlr_server_decoration_manager_create(server.display),
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	server.xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(server.display);
    server.xdg_decoration_listener.notify = xdg_new_decoration_notify;
	wl_signal_add(&server.xdg_decoration_mgr->events.new_toplevel_decoration, &server.xdg_decoration_listener);

	//CURSOR
	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	server.cursor_motion.notify = cursor_motion_notify;
	server.cursor_motion_absolute.notify = cursor_motion_absolute_notify;
	server.cursor_button.notify = cursor_button_notify;
	server.cursor_axis.notify = cursor_axis_notify;
	server.cursor_frame.notify = cursor_frame_notify;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_motion_absolute);
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	wl_list_init(&server.keyboards);
	server.new_input.notify = backend_new_input_notify;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);

	server.seat = wlr_seat_create(server.display, "seat0");
	server.request_cursor.notify = seat_request_set_cursor_notify;
	server.request_set_selection.notify = seat_request_set_selection_notify;
	wl_signal_add(&server.seat->events.request_set_cursor, &server.request_cursor);
	wl_signal_add(&server.seat->events.request_set_selection, &server.request_set_selection);

	//START BACKEND
	const char *socket = wl_display_add_socket_auto(server.display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	if (!wlr_backend_start(server.backend)) {
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.display);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);

	if (fork() == 0) {
		execl("/bin/sh", "/bin/sh", "-c", "alacritty", (void *)NULL);
		return 0;
	}
		

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(server.display);

	wl_display_destroy_clients(server.display);
	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wlr_output_layout_destroy(server.output_layout);
	wl_display_destroy(server.display);
	return 0;
}

int main() {
	struct wlr_box box = {0, 0, 1000, 1000};
	layout_init(box);
	waylandSetup();
}