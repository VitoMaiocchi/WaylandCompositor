#pragma once

#include "output.hpp"

class Titlebar {
    public:
    Titlebar(Extends ext);
    void updateExtends(Extends ext);

    private:
    Output::Buffer buffer;
    Extends extends;
};