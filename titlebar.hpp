#pragma once

#include "output.hpp"

class Titlebar {
    public:
    Titlebar(Extends ext);
    ~Titlebar();
    void updateExtends(Extends ext);
    void updateDesktop(uint desktop);

    static void setup();
    static int update_titlebars(void* data);

    private:
    void drawBuffer();

    Output::Buffer buffer;
    Extends extends;
    uint desktop = 0; 
};