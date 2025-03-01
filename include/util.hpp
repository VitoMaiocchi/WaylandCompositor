#pragma once
#include "wlr/util/box.h"
#include <exception>
#include <cassert>
#include <unistd.h> 
#include <format>
#include <map>
#include <sstream>

struct Point {
    int x = 0;
    int y = 0;
    
    Point() = default;
    Point(int x, int y) : x(x), y(y) {}
};

//TODO: Extends class mache wo denn Surface, Display, etc inherited
struct Extends : wlr_box {
    Extends() = default;
    Extends(const wlr_box& box) : wlr_box(box) {}
    Extends(int x, int y, int width, int height) : wlr_box({x,y,width,height}) {}

    Extends& setHeight(int height) {
        this->height = height;
        return *this;
    }

    Extends withHeight(int height) {
        return {this->x,this->y,this->width,height};
    }

    bool contains(int x, int y) {
        if(this->x > x) return false;
        if(this->x + this->width < x) return false;
        if(this->y > y) return false;
        if(this->y + this->height < y) return false;
        return true;
    }

    //limit Extends to constraint area
    Extends& constrain(Extends c) {
        if(this->height > c.height) this->height = c.height;
		if(this->width > c.width) this->width = c.width;

		if(this->x + this->width > c.x + c.width) this->x = c.x + c.width - this->width;
		else if(this->x < c.x) this->x = c.x;

		if(this->y + this->height > c.y + c.height) this->y = c.y + c.height - this->height;
		else if(this->y < c.y) this->y = c.y;
		return *this;
    }
};

//formatter for Extends
template <>
struct std::formatter<Extends> {
    // Parse the format string (e.g., "{}" or "{:x,y}")
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin(); // No custom formatting
    }

    // Format the MyClass object
    auto format(const Extends& ext, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "[x={},y={},width={},height={}]", ext.x,ext.y,ext.width,ext.height);
    }
};

namespace Logger {
    enum Importance {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        SILENT,
    };

    enum Category { //die immer au im util.cpp update
        WL_ROOTS,
        SURFACE,
        INPUT,
        OUTPUT,
        LAYOUT,
        UNCATEGORIZED,
        SERVER,
        LAUNCHER,
        categories_len
    };

    const Importance verbosity[categories_len] = {
       ERROR, //wlroots: hard coded
       DEBUG, //surface
       WARNING, //input
       DEBUG, //output
       ERROR, //layout
       DEBUG, //uncategorized
       INFO, //server
       DEBUG //launcher
    };

    void setup();
    void print_message(std::string message, Importance importance, Category category, const char* file, int line);
    inline void log_message(std::string message, Importance importance, Category category, const char* file, int line) {
        if(importance < verbosity[category]) return;
        assert(importance < SILENT);
        print_message(message, importance, category, file, line);
    }
}

#ifdef LOGGER_CATEGORY
#define log(importance, message, ...) \
    Logger::log_message(std::format(message, ##__VA_ARGS__), importance, LOGGER_CATEGORY, __FILE__, __LINE__)
#else
#define log(importance, message, ...) \
    Logger::log_message(std::format(message, ##__VA_ARGS__), importance, Logger::UNCATEGORIZED, __FILE__, __LINE__)
#endif

#define debug(message, ...) \
    log(Logger::DEBUG, message, ##__VA_ARGS__)

#define info(message, ...) \
    log(Logger::INFO, message, ##__VA_ARGS__)

#define warn(message, ...) \
    log(Logger::WARNING, message, ##__VA_ARGS__)

#define error(message, ...) \
    log(Logger::ERROR, message, ##__VA_ARGS__)



inline void spawn(const char* cmd) {
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
        return;
    }
}

constexpr const char* UNIXTERM_GRAY = "\033[90m";
constexpr const char* UNIXTERM_BLUE = "\033[34m";
constexpr const char* UNIXTERM_YELLOW = "\033[1;33m";
constexpr const char* UNIXTERM_RED = "\033[31m";
constexpr const char* UNIXTERM_BOLD = "\033[1m";
constexpr const char* UNIXTERM_BOLDRED = "\033[31m\033[1m";
constexpr const char* UNIXTERM_RESET = "\033[0m";