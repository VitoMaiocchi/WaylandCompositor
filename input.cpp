#include "input.hpp"
#include "includes.hpp"
#include "wayland.hpp"
#include "layout.hpp"

namespace Input {

    struct WaylandKeyboard {
        struct wl_list link;
        struct wlr_keyboard *wlr_keyboard;

        struct wl_listener modifiers;
        struct wl_listener key;
        struct wl_listener destroy;
    };

    // INPUT HANDLING
    void cursor_process_movement(uint32_t time) {
        ToplevelSurface* surface = Layout::getSurfaceAtPosition(server.cursor->x, server.cursor->y);

        if(!surface) {
            if(server.pointer_focus) {
                wlr_seat_pointer_clear_focus(server.seat);
                server.pointer_focus = NULL;
            }
            return;
        }
        
        std::pair<int, int> cursor_cords = surface->surfaceCoordinateTransform(server.cursor->x, server.cursor->y);

        if(server.pointer_focus != surface) {
            if(server.pointer_focus) wlr_seat_pointer_clear_focus(server.seat);
            server.pointer_focus = surface;
            wlr_seat_pointer_notify_enter(server.seat, surface->getSurface(), cursor_cords.first, cursor_cords.second);
            surface->setFocused();
        }

        wlr_seat_pointer_notify_motion(server.seat, time, cursor_cords.first, cursor_cords.second);
        
    }

    void cursor_motion_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_motion_event *event = (wlr_pointer_motion_event *) data;
        wlr_cursor_move(server.cursor, &event->pointer->base, event->delta_x, event->delta_y);
        cursor_process_movement(event->time_msec);
        wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default"); //TODO: di göhret nöd da ane
    }

    void cursor_motion_absolute_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_motion_absolute_event *event = (wlr_pointer_motion_absolute_event *) data;
        wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);
        cursor_process_movement(event->time_msec);
        wlr_cursor_set_xcursor(server.cursor, server.cursor_mgr, "default");
    }

    void cursor_button_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_button_event *event = (wlr_pointer_button_event *) data;
        wlr_seat_pointer_notify_button(server.seat, event->time_msec, event->button, event->state);
    }

    void cursor_axis_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_axis_event *event = (wlr_pointer_axis_event*) data;
        wlr_seat_pointer_notify_axis(server.seat, event->time_msec, event->orientation, 
                event->delta, event->delta_discrete, event->source, event->relative_direction);
    }

    void cursor_frame_notify(struct wl_listener *listener, void *data) {
        wlr_seat_pointer_notify_frame(server.seat);
    }

    void seat_request_set_cursor_notify(struct wl_listener *listener, void *data) {
        //client requests set cursor image
        struct wlr_seat_pointer_request_set_cursor_event *event = (wlr_seat_pointer_request_set_cursor_event *) data;
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
        struct wlr_seat_request_set_selection_event *event = (wlr_seat_request_set_selection_event *) data;
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
        wlr_log(WLR_DEBUG, "KEY PRESSED = %i", sym);
        if(!(modmask & WLR_MODIFIER_LOGO)) return false;

        wlr_log(WLR_DEBUG, "WITH WIN KEY PRESSED = %i", sym);

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
        struct wlr_keyboard_key_event *event = (wlr_keyboard_key_event *)data;
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

        struct WaylandKeyboard *keyboard = (WaylandKeyboard *) calloc(1, sizeof(*keyboard));
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
        struct wlr_input_device *device = (wlr_input_device *) data;

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

    void setup() {
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
    }
}