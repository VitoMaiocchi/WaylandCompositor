#include "server.hpp"
#include "util.hpp"
#include "input.hpp"
#include "layout.hpp"

//TODO: mer chönt da theoretisch no en 10 desktop adde
void change_desktop(xkb_keysym_t sym, uint32_t modmask) {
	uint desktop = sym - XKB_KEY_1;
	Layout::setDesktop(desktop);
}

int main() {

	Input::registerKeyCallback(XKB_KEY_o, WLR_MODIFIER_LOGO, [](xkb_keysym_t sym, uint32_t modmask){
		spawn("konsole");
	});

	Input::registerKeyCallback(XKB_KEY_Q, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, [](xkb_keysym_t sym, uint32_t modmask){
		wl_display_terminate(Server::display);
	});

	for(xkb_keysym_t key = XKB_KEY_1; key <= XKB_KEY_9; key++) Input::registerKeyCallback(key, WLR_MODIFIER_LOGO, change_desktop);

	Server::setup();
}