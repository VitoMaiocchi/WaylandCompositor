#include "includes.hpp"
#include "config.hpp"
#include "wayland.hpp"
#include "surface.hpp"
#include "layout.hpp"
#include "buffer.hpp"
#include "output.hpp"
#include "input.hpp"

WaylandServer server;

#include <cairo/cairo.h>
#include <cassert>
#include <random>
#include <memory>
#include "titlebar.hpp"

void draw(cairo_t* cr) {
	std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_real_distribution<> distr(0.0, 1.0); // Define the range

	cairo_set_source_rgb(cr, distr(gen), distr(gen), distr(gen));
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 200.0, 25.0);
	cairo_show_text (cr, "Ich habe gerade 100'000 Euro auf bravolotto gewonnen");
}

std::unique_ptr<Buffer> buffer;

void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
	wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

	buffer->draw(draw, 1000, 30);
}



/*
        struct WaylandClient* client = calloc(1, sizeof(*client));
        client->xdg_toplevel = xdg_surface->toplevel;
        client->root_node = wlr_scene_tree_create(&server.scene->tree);
        client->surface_node = wlr_scene_xdg_surface_create(client->root_node, xdg_surface);
        xdg_surface->data = client->surface_node;
        client->xdg_surface = xdg_surface;

        wlr_scene_node_set_position(&client->root_node->node, extends.x, extends.y);
        wlr_xdg_toplevel_set_size(client->xdg_toplevel, extends.width-2*BORDERWIDTH, extends.height-2*BORDERWIDTH);
        wlr_scene_node_set_position(&client->surface_node->node, BORDERWIDTH, BORDERWIDTH);
*/

struct LayerShellSurface {
	struct wl_list link;
	struct wlr_layer_surface_v1* ls_surface; 
	struct wlr_scene_layer_surface_v1* surface_node;
};

struct wl_list lss_list;


//LAYER SHELL ULTRA BASIC
/*
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
*/

void layer_shell_delete_surface(struct wl_listener *listener, void *data) {
	//idk ob ich da was will
}

	//SETUP

#define ASSERT_NOTNULL(n, m) if(!n) {   \
    wlr_log(WLR_ERROR, m);              \
    return 1;                           \
}


bool waylandSetup() {
    wlr_log_init(WLR_DEBUG, NULL);

    server.display = wl_display_create();

	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.display), NULL);
    ASSERT_NOTNULL(server.backend, "failed to create wlr_backend");

	server.renderer = wlr_renderer_autocreate(server.backend);
    ASSERT_NOTNULL(server.renderer, "failed to create wlr_renderer");
	wlr_renderer_init_wl_display(server.renderer, server.display);

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    ASSERT_NOTNULL(server.allocator, "failed to create wlr_allocator");

	wlr_compositor_create(server.display, 5, server.renderer);
	wlr_subcompositor_create(server.display);
	wlr_data_device_manager_create(server.display);

	Output::setup();

	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

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
	
	wlr_log(WLR_DEBUG, "AHHH");

	wlr_log(WLR_DEBUG, "server 1: %p", &server);
	wlr_log(WLR_DEBUG, "aaah place: %p", &server.display);
	wlr_log(WLR_DEBUG, "aaah value: %p", server.display);
	//server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
	setupSurface();

    //requesting serverside decorations of clients
	wlr_server_decoration_manager_set_default_mode(
			wlr_server_decoration_manager_create(server.display),
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	server.xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(server.display);
    server.xdg_decoration_listener.notify = xdg_new_decoration_notify;
	wl_signal_add(&server.xdg_decoration_mgr->events.new_toplevel_decoration, &server.xdg_decoration_listener);

	Input::setup();

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


	//CAIRO 
	buffer = std::make_unique<Buffer>(&server.scene->tree);
	buffer->setPosition(0,0);

	//CAIRO END

	setenv("WAYLAND_DISPLAY", socket, true);

	if (fork() == 0) {
		execl("/bin/sh", "/bin/sh", "-c", "foot", (void *)NULL);
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
	//layout_init(box);
	waylandSetup();
}