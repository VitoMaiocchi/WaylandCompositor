#include "server.hpp"
#include "includes.hpp"
#include "output.hpp"
#include "surface.hpp"
#include "input.hpp"

#define ASSERT_NOTNULL(n, m) if(!n) {   \
    wlr_log(WLR_ERROR, m);              \
    return 1;                           \
}

namespace Server {
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
        setupSurface();
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