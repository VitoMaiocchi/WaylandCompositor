#include "titlebar.hpp"
#include "server.hpp"
#include <iostream>
#include <list>
#include <algorithm>
#include <ctime>
#include <optional>
#include <fstream>
#include <filesystem>

#define UPDATE_FREQUENCY 500

std::string getCurrentTimeAndDate();
std::string getBatteryStatus();

std::list<Titlebar*> titlebars;
std::string current_time = "";
std::string battery_status = "";
std::string volume = "";

bool has_bat = false;
std::string ac_path = "";
std::string bat_path = "";

void Titlebar::updateVolume(uint volume_percent) {
    volume = "Volume: "+std::to_string(volume_percent)+"%";
    for(Titlebar* titlebar : titlebars) titlebar->drawBuffer();
}

int Titlebar::update_titlebars(void* data) {
    current_time = getCurrentTimeAndDate();
    battery_status = getBatteryStatus();

    for(Titlebar* titlebar : titlebars) titlebar->drawBuffer();

    wl_event_loop* event_loop = (wl_event_loop*) data;
    wl_event_source* source = wl_event_loop_add_timer(event_loop, update_titlebars, event_loop);
    wl_event_source_timer_update(source, UPDATE_FREQUENCY);
    return 0;
}

void Titlebar::setup() {
    //find ac and battery path
    const std::string path = "/sys/class/power_supply/";
    if(std::filesystem::exists(path+"BAT")) bat_path = path+"BAT";
    else if(std::filesystem::exists(path+"BAT0")) bat_path = path+"BAT0";
    else if(std::filesystem::exists(path+"BAT1")) bat_path = path+"BAT1";
    if(std::filesystem::exists(path+"AC")) ac_path = path+"AC";
    else if(std::filesystem::exists(path+"AC0")) ac_path = path+"AC0";
    else if(std::filesystem::exists(path+"AC1")) ac_path = path+"AC1";
    if(bat_path != "") has_bat = true;

    //setup update event
    wl_event_loop* event_loop = wl_display_get_event_loop(Server::display);
    update_titlebars(event_loop);
}

Titlebar::Titlebar(Extends ext) : extends(ext) {
    drawBuffer();
    titlebars.push_back(this);
}

Titlebar::~Titlebar() {
    auto it = std::find(titlebars.begin(), titlebars.end(), this);
    assert(it != titlebars.end());
    titlebars.erase(it);
}

void Titlebar::updateExtends(Extends ext) {
    extends = ext;
    drawBuffer();
}


void Titlebar::updateDesktop(uint desktop) {
    this->desktop = desktop;
    drawBuffer();
}

void draw(cairo_t* cr, uint desktop) {
	cairo_set_source_rgb(cr, 0.2, 0.8, 0.6);
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

    std::string text = current_time +"   Desktop: "+ std::to_string(desktop+1)+
        "   "+battery_status + "   "+volume;

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 20.0, 25.0);
	cairo_show_text (cr, text.c_str());
}

std::function<void(cairo_t*)> draw_func(uint desktop) {
    return [desktop](cairo_t* cr) {
        draw(cr, desktop);
    };
}

void Titlebar::drawBuffer() {
    buffer.draw(draw_func(desktop), extends);
}

//UTIL

std::string getCurrentTimeAndDate() {
    std::time_t now = std::time(nullptr);
    std::tm localTime = *std::localtime(&now);

    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(2) << localTime.tm_hour << ":"
        << std::setw(2) << localTime.tm_min << ":"
        << std::setw(2) << localTime.tm_sec << " "
        << std::setw(2) << localTime.tm_mday << "."
        << std::setw(2) << (localTime.tm_mon + 1) << "."
        << (localTime.tm_year + 1900);
    
    return oss.str();
}

std::string readFileContents(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::in);
    if(!file.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

std::string getBatteryStatus() {
    if(!has_bat) return "";
    std::string status = "";
    int ac = std::stoi(readFileContents(ac_path+"/online"));
    int bat = std::stoi(readFileContents(bat_path+"/capacity"));

    if(ac)  status += "AC: ";
    else    status += "BAT: ";
    status += std::to_string(bat)+"%"; //TODO: add leading 0 (formatting)

    return status;
}
