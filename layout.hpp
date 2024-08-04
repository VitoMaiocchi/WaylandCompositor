#ifndef VITOWM_LAYOUT_H
#define VITOWM_LAYOUT_H

#include "includes.hpp"
#include "surface.h"

namespace Layout {

void setScreenExtends(wlr_box extends); //TMP

//adds surface to layout
void addSurface(ToplevelSurface* surface);
//removes surface from layout. Returns NULL or the next surface to focus
//EIG CHÖNT MER DA AU void return und direkt focus da handle
ToplevelSurface* removeSurface(ToplevelSurface* surface); 
//returns topmost surface at given position or NULL
ToplevelSurface* getSurfaceAtPosition(const int x, const int y);

}

#endif