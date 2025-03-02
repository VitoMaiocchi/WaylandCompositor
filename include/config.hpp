#pragma once

#define BORDERWIDTH 2

#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

const float bordercolor_inactive[] = COLOR(0x777777FF);
const float bordercolor_active[] = COLOR(0xFF0000FF);

#define REPEATKEY_DELAY 400
#define REPEATKEY_FREQUENCY 50