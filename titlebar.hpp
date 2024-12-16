#pragma once

#include "output.hpp"

class Titlebar {
    public:
    Titlebar(Extends ext);
    void updateExtends(Extends ext);
    void updateDesktop(uint desktop);

    private:
    void drawBuffer();

    Output::Buffer buffer;
    Extends extends;
    uint desktop = 0; 
};