#include "titlebar.hpp"

void draw(cairo_t* cr) {
	cairo_set_source_rgb(cr, 0.2, 0.8, 0.6);
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 200.0, 25.0);
	cairo_show_text (cr, "Ich habe gerade 100'000 Euro auf bravolotto gewonnen");
}

Titlebar::Titlebar(Extends ext) : extends(ext) {
		buffer.draw(draw, extends);
}

void Titlebar::updateExtends(Extends ext) {
    extends = ext;
    buffer.draw(draw, extends);
}