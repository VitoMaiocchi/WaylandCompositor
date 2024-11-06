#include "server.hpp"
#include "util.hpp"
#include "input.hpp"

int main() {

	Input::registerKeyCallback(XKB_KEY_o, WLR_MODIFIER_LOGO, [](xkb_keysym_t sym, uint32_t modmask){
		spawn("konsole");
	});

	Input::registerKeyCallback(XKB_KEY_Q, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, [](xkb_keysym_t sym, uint32_t modmask){
		wl_display_terminate(Server::display);
	});

	Server::setup();
}