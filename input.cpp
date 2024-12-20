#include "input.hpp"
#include "includes.hpp"
#include "layout.hpp"
#include "server.hpp"
#include "output.hpp"
#include <map>

namespace Input {

    wlr_cursor* cursor;
	wlr_xcursor_manager* cursor_mgr;
	Surface::Base* pointer_focus;
	wl_listener cursor_motion;
	wl_listener cursor_motion_absolute;
	wl_listener cursor_button;
	wl_listener cursor_axis;
	wl_listener cursor_frame;

    wlr_seat* seat;

	wl_listener new_input;
	wl_listener request_cursor;
	wl_listener request_set_selection;
	wl_list keyboards;

    struct WaylandKeyboard {
        wl_list link;
        wlr_keyboard *wlr_keyboard;

        wl_listener modifiers;
        wl_listener key;
        wl_listener destroy;
    };

    void setKeyboardFocus(Surface::Base* surface) {
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(Input::seat);
		if (keyboard) wlr_seat_keyboard_notify_enter(Input::seat, surface->getSurface(),
				keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }

    void setCursorFocus(Surface::Base* surface) {
        if(!surface) {
            if(pointer_focus) {
                wlr_seat_pointer_clear_focus(seat);
                pointer_focus = NULL;
            }
            return;
        }

        if(pointer_focus != surface) {
            if(pointer_focus) wlr_seat_pointer_clear_focus(seat);
            pointer_focus = surface;
            std::pair<int, int> cursor_cords = surface->surfaceCoordinateTransform(cursor->x, cursor->y);
            wlr_seat_pointer_notify_enter(seat, surface->getSurface(), cursor_cords.first, cursor_cords.second);
        }
    }

    // INPUT HANDLING
    void cursor_process_movement(uint32_t time) {
        Layout::handleCursorMovement(cursor->x, cursor->y);
        if(!pointer_focus) return;
        
        std::pair<int, int> cursor_cords = pointer_focus->surfaceCoordinateTransform(cursor->x, cursor->y);
        wlr_seat_pointer_notify_motion(seat, time, cursor_cords.first, cursor_cords.second);
    }

    void cursor_motion_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_motion_event *event = (wlr_pointer_motion_event *) data;
        wlr_cursor_move(cursor, &event->pointer->base, event->delta_x, event->delta_y);
        cursor_process_movement(event->time_msec);
        wlr_cursor_set_xcursor(cursor, cursor_mgr, "default"); //TODO: di göhret nöd da ane
    }

    void cursor_motion_absolute_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_motion_absolute_event *event = (wlr_pointer_motion_absolute_event *) data;
        wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);
        cursor_process_movement(event->time_msec);
        wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    }

    void cursor_button_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_button_event *event = (wlr_pointer_button_event *) data;
        wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
    }

    //SCROLL EVENT
    //TODO: add touchpad scroll aceleration
    void cursor_axis_notify(struct wl_listener *listener, void *data) {
        struct wlr_pointer_axis_event *event = (wlr_pointer_axis_event*) data;
        wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation, 
                event->delta, event->delta_discrete, event->source, event->relative_direction);
    }

    void cursor_frame_notify(struct wl_listener *listener, void *data) {
        wlr_seat_pointer_notify_frame(seat);
    }

    void seat_request_set_cursor_notify(struct wl_listener *listener, void *data) {
        //client requests set cursor image
        struct wlr_seat_pointer_request_set_cursor_event *event = (wlr_seat_pointer_request_set_cursor_event *) data;
        struct wlr_seat_client *focused_client = seat->pointer_state.focused_client;
        /* This can be sent by any client, so we check to make sure this one is
        * actually has pointer focus first. */
        if (focused_client == event->seat_client) {
            /* Once we've vetted the client, we can tell the cursor to use the
            * provided surface as the cursor image. It will set the hardware cursor
            * on the output that it's currently on and continue to do so as the
            * cursor moves between outputs. */
            wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x, event->hotspot_y);
        }
    }

    void seat_request_set_selection_notify(struct wl_listener *listener, void *data) {
        struct wlr_seat_request_set_selection_event *event = (wlr_seat_request_set_selection_event *) data;
        wlr_seat_set_selection(seat, event->source, event->serial);
    }

    void add_new_pointer_device(struct wlr_input_device* device) {
        wlr_cursor_attach_input_device(cursor, device);
    }


    //KEYBOARD

    std::map<uint64_t, ShortcutCallback> shortcut_callbacks; 

    inline uint64_t getShortCutHash(xkb_keysym_t sym, uint32_t modmask) {
        return ((uint64_t)modmask << 32) | sym;
    }

    void registerKeyCallback(xkb_keysym_t sym, uint32_t modmask, ShortcutCallback callback) {
        shortcut_callbacks[getShortCutHash(sym, modmask)] = callback;
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
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        /* Send modifiers to the client. */
        wlr_seat_keyboard_notify_modifiers(seat, &keyboard->wlr_keyboard->modifiers);
    }

    //return true if the keybind exists
    static bool handle_keybinding(xkb_keysym_t sym, uint32_t modmask) {
        auto it = shortcut_callbacks.find(getShortCutHash(sym, modmask));
        if(it == shortcut_callbacks.end()) return false;
        it->second(sym, modmask);
        return true;
    }

    static void keyboard_handle_key(struct wl_listener *listener, void *data) {
        /* This event is raised when a key is pressed or released. */
        struct WaylandKeyboard *keyboard = wl_container_of(listener, keyboard, key);
        struct wlr_keyboard_key_event *event = (wlr_keyboard_key_event *)data;

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
            assert(keyboard);
            assert(keyboard->wlr_keyboard);
            assert(seat);
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


        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        wl_list_insert(&keyboards, &keyboard->link);
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
        if (!wl_list_empty(&keyboards)) caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_seat_set_capabilities(seat, caps);
    }

    void setup() {
        cursor = wlr_cursor_create();
        wlr_cursor_attach_output_layout(cursor, Output::output_layout);

        cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

        cursor_motion.notify = cursor_motion_notify;
        cursor_motion_absolute.notify = cursor_motion_absolute_notify;
        cursor_button.notify = cursor_button_notify;
        cursor_axis.notify = cursor_axis_notify;
        cursor_frame.notify = cursor_frame_notify;
        wl_signal_add(&cursor->events.motion, &cursor_motion);
        wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
        wl_signal_add(&cursor->events.button, &cursor_button);
        wl_signal_add(&cursor->events.axis, &cursor_axis);
        wl_signal_add(&cursor->events.frame, &cursor_frame);


        wl_list_init(&keyboards);
        new_input.notify = backend_new_input_notify;
        wl_signal_add(&Server::backend->events.new_input, &new_input);

        seat = wlr_seat_create(Server::display, "seat0");
        request_cursor.notify = seat_request_set_cursor_notify;
        request_set_selection.notify = seat_request_set_selection_notify;
        wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
        wl_signal_add(&seat->events.request_set_selection, &request_set_selection);
    }

    void cleanup() {
        wlr_xcursor_manager_destroy(cursor_mgr);
    }
}