WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=\
	 $(shell pkg-config --cflags --libs "wlroots >= 0.17.0-dev") \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon)

FLAGS=$(CFLAGS) -Wall -I/usr/include/pixman-1 -I. -DWLR_USE_UNSTABLE

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		wlr-layer-shell-unstable-v1.xml $@

wayland.o: wayland.c layout.h xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h
	$(CC) -c $< $(FLAGS)

layout.o: layout.c layout.h wayland.h config.h
	$(CC) -c $< $(FLAGS)

VitoWM: wayland.o layout.o config.h
	$(CC) -o VitoWM $^ $(LIBS)

clean:
	rm -f tinywl VitoWM xdg-shell-protocol.h xdg-shell-protocol.c

.DEFAULT_GOAL=VitoWM
.PHONY: clean
