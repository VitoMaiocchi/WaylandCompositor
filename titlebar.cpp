#include "titlebar.hpp"
#include "wayland.hpp"

#include <random> //NUR DEBUG NÖD NÖTIG

Titlebar::Titlebar(wlr_box ext) : buffer(&server.scene->tree) {
    extends = ext;
    buffer.setPosition(ext.x, ext.y);
    buffer.draw([this](cairo_t* cr){this->draw(cr);}, ext.width, ext.height);
}

void Titlebar::updateExtends(wlr_box ext) {
    extends = ext;
    buffer.setPosition(ext.x, ext.y);
    buffer.draw([this](cairo_t* cr){this->draw(cr);}, ext.width, ext.height);
}

void Titlebar::draw(cairo_t* cr) {
	std::random_device rd;  
    std::mt19937 gen(rd()); 
    std::uniform_real_distribution<> distr(0.0, 1.0);

	cairo_set_source_rgb(cr, distr(gen), distr(gen), distr(gen));
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 200.0, 25.0);
	cairo_show_text (cr, "Ich habe gerade 100'000 Euro auf bravolotto gewonnen");
}