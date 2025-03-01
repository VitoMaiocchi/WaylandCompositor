#pragma once

#include <xkbcommon/xkbcommon.h>

namespace Launcher {
    void setup();
    void run();
    void keyPressEvent(xkb_keysym_t sym);
    bool isRunning();
}