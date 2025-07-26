#pragma once

#include "output.hpp"

class Titlebar {
    public:
    Titlebar(Extends ext);
    ~Titlebar();
    void updateExtends(Extends ext);
    void updateDesktop(uint desktop);
    void setVisibility(bool visible);

    static void setup();
    static int update_titlebars(void* data);
    static void updateVolume(uint volume_percent);
    static void updateAudioMute(bool muted);
    static void updateDebugText(std::string s);

    private:
    void drawBuffer();

    Output::Buffer buffer;
    Extends extends;
    uint desktop = 0; 
};