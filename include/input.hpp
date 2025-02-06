#pragma once

#include "includes.hpp"
#include "surface.hpp"
#include <functional>

typedef std::function<void(xkb_keysym_t sym, uint32_t modmask)> ShortcutCallback;

namespace Input {
    extern wlr_seat* seat;

    extern wlr_xcursor_manager* cursor_mgr; //nur temporär

    void setup();
    void cleanup();

    void registerKeyCallback(xkb_keysym_t sym, uint32_t modmask, ShortcutCallback callback);
    void setCursorFocus(Surface::Toplevel* surface);
    void setKeyboardFocus(Surface::Toplevel* surface);
}