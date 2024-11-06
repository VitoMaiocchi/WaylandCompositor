#ifndef VITOWM_LAYOUT_H
#define VITOWM_LAYOUT_H

#include "includes.hpp"
#include "surface.hpp"

namespace Layout {

    void setScreenExtends(wlr_box extends); //TMP

    //adds surface to layout
    void addSurface(Surface::Toplevel* surface);
    //removes surface from layout. Returns NULL or the next surface to focus
    //EIG CHÖNT MER DA AU void return und direkt focus da handle
    Surface::Toplevel* removeSurface(Surface::Toplevel* surface); 
    //returns topmost surface at given position or NULL
    Surface::Toplevel* getSurfaceAtPosition(const int x, const int y);

}

#endif