#ifndef VITOWM_LAYOUT_H
#define VITOWM_LAYOUT_H

#include "includes.hpp"
#include "surface.hpp"
#include "output.hpp"
#include <memory>

namespace Layout {
    //adds surface to layout
    void addSurface(Surface::Toplevel* surface);
    //removes surface from layout.
    void removeSurface(Surface::Toplevel* surface); 

    void removeDisplay(Output::Display* display);

    void handleCursorMovement(const double x, const double y);

    //abstract base class for different layout types 
    class Base {
        public:
            Base(Extends ext);
            void updateExtends(Extends ext);
            virtual void addSurface(Surface::Toplevel* surface) = 0;
            virtual Surface::Toplevel* removeSurface(Surface::Toplevel* surface) = 0;

        protected:
            Extends extends;
            virtual void updateLayout() = 0;
    };

    std::unique_ptr<Base> generateNewLayout(Extends ext);
}

#endif