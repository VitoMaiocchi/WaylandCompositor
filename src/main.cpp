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

	Input::registerKeyCallback(XKB_KEY_p, WLR_MODIFIER_LOGO, [](xkb_keysym_t sym, uint32_t modmask){
		spawn("vlc");
	});

	Input::registerKeyCallback(XKB_KEY_q, WLR_MODIFIER_LOGO, [](xkb_keysym_t sym, uint32_t modmask){
		Layout::killClient();
	});

	//FIXME: das gaht nöd wenn es xwayland fenster offe isch
	Input::registerKeyCallback(XKB_KEY_Q, WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, [](xkb_keysym_t sym, uint32_t modmask){
		Server::connected = false;
	});

	Input::registerKeyCallback(XKB_KEY_Escape, WLR_MODIFIER_LOGO, [](xkb_keysym_t sym, uint32_t modmask){
		Server::connected = false;
	});

	for(xkb_keysym_t key = XKB_KEY_1; key <= XKB_KEY_9; key++) Input::registerKeyCallback(key, WLR_MODIFIER_LOGO, change_desktop);

	Input::registerKeyCallback(XKB_KEY_XF86AudioRaiseVolume, 0, [](xkb_keysym_t sym, uint32_t modmask){
		Output::adjustVolume(0.02);
	});

	Input::registerKeyCallback(XKB_KEY_XF86AudioLowerVolume, 0, [](xkb_keysym_t sym, uint32_t modmask){
		Output::adjustVolume(-0.02);
	});

	Input::registerKeyCallback(XKB_KEY_XF86AudioMute, 0, [](xkb_keysym_t sym, uint32_t modmask){
		Output::muteVolume();
	});

	Server::setup();
}