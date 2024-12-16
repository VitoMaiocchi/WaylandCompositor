#include "titlebar.hpp"

Titlebar::Titlebar(Extends ext) : extends(ext) {
    drawBuffer();
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

    std::string text = std::to_string(desktop+1) + " : Ich habe gerade 100'000 Euro auf bravolotto gewonnen";

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 200.0, 25.0);
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