#ifndef VITOWM_LAYOUT_H
#define VITOWM_LAYOUT_H

#include "includes.hpp"
#include "surface.hpp"

namespace Layout {

    void setScreenExtends(wlr_box extends); //TMP

    //adds surface to layout
    void addSurface(Surface::Toplevel* surface);
    //removes surface from layout.
    void removeSurface(Surface::Toplevel* surface); 

    void handleCursorMovement(const double x, const double y);
}

#endif