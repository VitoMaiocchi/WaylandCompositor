#pragma once

#include "includes.hpp"
#include "surface.hpp"
#include "output.hpp"
#include "titlebar.hpp"
#include <memory>

namespace Layout {

    void addSurface(Surface::Toplevel* surface);
    void removeSurface(Surface::Toplevel* surface);
    void handleCursorMovement(const double x, const double y);
    void setDesktop(uint desktop);

    struct Fullscreen;

    //logical representation of one or multiple mirrored monitors
    struct Display {
        Display(Extends ext);
        ~Display();
        void updateExtends(Extends ext);

        std::unique_ptr<Fullscreen> fullscreen;
    };
}