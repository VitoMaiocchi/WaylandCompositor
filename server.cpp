#include "server.hpp"
#include "includes.hpp"
#include "output.hpp"
#include "surface.hpp"
#include "input.hpp"

#include <random> //nur debug

namespace Server {
    void xdg_new_decoration_notify(struct wl_listener *listener, void *data) {
        struct wlr_xdg_toplevel_decoration_v1 *dec = (wlr_xdg_toplevel_decoration_v1*) data;
        wlr_xdg_toplevel_decoration_v1_set_mode(dec, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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

    void layer_shell_delete_surface(struct wl_listener *listener, void *data) {
        //idk ob ich da was will
    }
    */

        //SETUP

    #define ASSERT_NOTNULL(n, m) if(!n) {   \
        wlr_log(WLR_ERROR, m);              \
        return 1;                           \
    }

    //das chammer irgend wenn no da weg mache
    wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
    wl_listener xdg_decoration_listener;

    wl_display* display;
	wlr_backend* backend;
    wlr_renderer* renderer;
	wlr_allocator* allocator;

    bool setup() {
        wlr_log_init(WLR_DEBUG, NULL);

        display = wl_display_create();

        backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
        ASSERT_NOTNULL(backend, "failed to create wlr_backend");

        renderer = wlr_renderer_autocreate(backend);
        ASSERT_NOTNULL(renderer, "failed to create wlr_renderer");
        wlr_renderer_init_wl_display(renderer, display);

        allocator = wlr_allocator_autocreate(backend, renderer);
        ASSERT_NOTNULL(allocator, "failed to create wlr_allocator");

        wlr_compositor_create(display, 5, renderer);
        wlr_subcompositor_create(display);
        wlr_data_device_manager_create(display);


        Output::setup();

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
        
        setupSurface();

        //requesting serverside decorations of clients
        wlr_server_decoration_manager_set_default_mode(
                wlr_server_decoration_manager_create(display),
                WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
        xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(display);
        xdg_decoration_listener.notify = xdg_new_decoration_notify;
        wl_signal_add(&xdg_decoration_mgr->events.new_toplevel_decoration, &xdg_decoration_listener);

        Input::setup();


        //START BACKEND
        const char *socket = wl_display_add_socket_auto(display);
        if (!socket) {
            wlr_backend_destroy(backend);
            return 1;
        }

        if (!wlr_backend_start(backend)) {
            wlr_backend_destroy(backend);
            wl_display_destroy(display);
            return 1;
        }

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
        wl_display_run(display);

        wl_display_destroy_clients(display);
        Output::cleanup();
        Input::cleanup();
        wl_display_destroy(display);
        return 0;
    }
}