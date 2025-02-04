#define LOGGER_CATEGORY Logger::SERVER
#include "server.hpp"
#include "includes.hpp"
#include "output.hpp"
#include "surface.hpp"
#include "input.hpp"
#include "titlebar.hpp"

#include <queue>
#include <mutex>
#include <iostream>
#include <chrono>

#define ASSERT_NOTNULL(n, m) if(!n) {   \
    error(m);                           \
    return 1;                           \
}

namespace Server {
    wl_display* display;
	wlr_backend* backend;
    wlr_renderer* renderer;
	wlr_allocator* allocator;
    wlr_compositor* compositor;
    bool connected = false;

    void dispatchEvents();


    bool setup() {
        Logger::setup();
        display = wl_display_create();

        backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
        ASSERT_NOTNULL(backend, "failed to create wlr_backend");

        renderer = wlr_renderer_autocreate(backend);
        ASSERT_NOTNULL(renderer, "failed to create wlr_renderer");
        wlr_renderer_init_wl_display(renderer, display);

        allocator = wlr_allocator_autocreate(backend, renderer);
        ASSERT_NOTNULL(allocator, "failed to create wlr_allocator");

        compositor = wlr_compositor_create(display, 5, renderer);
        ASSERT_NOTNULL(compositor, "failed to create wlr_compositor");
        wlr_subcompositor_create(display);
        wlr_data_device_manager_create(display);


        Output::setup();
        Surface::setupXdgShell();
        Surface::setupXWayland();
        Input::setup();
        Titlebar::setup();


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


        /* Run the Wayland event loop. This does not return until you exit the
        * compositor. Starting the backend rigged up all of the necessary event
        * loop configuration to listen to libinput events, DRM events, generate
        * frame events at the refresh rate, and so on. */
        info("Starting wayland compositor on WAYLAND_DISPLAY={}", socket);
        wl_event_loop* event_loop = wl_display_get_event_loop(display);

        auto last = std::chrono::steady_clock::now();

        Input::setCursorFocus(nullptr);

        //TODO: move loop to main.cpp and add SIGINT
        connected = true;
        while (connected) {
            wl_display_flush_clients(display);
            if (wl_event_loop_dispatch(event_loop, 50) < 0) break;
            dispatchEvents();
            auto now = std::chrono::steady_clock::now();
            //auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
            last = now;
        }

        wl_display_destroy_clients(display);
        Output::cleanup();
        Input::cleanup();
        wl_display_destroy(display);
        return 0;
    }

    struct Event {
        EventCallback callback;
        void* data;
    };

    std::queue<Event> eventQueue;
    std::mutex eventQueueMut;

    void dispatchEvents() {
        eventQueueMut.lock();
        while(!eventQueue.empty()) {
            Event &event = eventQueue.front();
            event.callback(event.data);
            eventQueue.pop();
        }
        eventQueueMut.unlock();
    }

    void queueEvent(EventCallback callback, void* data) {
        eventQueueMut.lock();
        eventQueue.push({callback, data});
        eventQueueMut.unlock();
    }
}