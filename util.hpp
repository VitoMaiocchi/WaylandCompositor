#pragma once
#include "wlr/util/box.h"

typedef wlr_box Extends;

inline void spawn(const char* cmd) {
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
        return;
    }
}

