#include "util.hpp"
#include "includes.hpp"

#include <iostream>

namespace Logger {
    const char* color_code[4] = {
        UNIXTERM_GRAY,
        UNIXTERM_BLUE,
        UNIXTERM_YELLOW,
        UNIXTERM_BOLDRED
    };

    const char* importance_text[4] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR"
    };

    const char* category_text[] = {
        " - wl-roots",
        " - surface",
        " - input",
        " - output",
        " - layout",
        "",
        " - wayland-server",
        " - launcher"
    };

    void print_message(std::string message, Importance importance, Category category, const char* file, int line) {
        std::cout << color_code[importance]<<"["<<importance_text[importance]<<category_text[category]<<"] "<<
        message <<UNIXTERM_GRAY<< "\n     -> at "<<file<<":"<<line<< UNIXTERM_RESET << std::endl;
    };

    void wlr_log_callback(enum wlr_log_importance importance, const char *fmt, va_list args) {
        if(importance != WLR_ERROR) return;
        std::cout << color_code[ERROR] << "[ERROR - wl-roots]"
        << fmt
        << UNIXTERM_RESET << std::endl;
    }

    void setup() {
        wlr_log_init(WLR_ERROR, wlr_log_callback);
    }
}