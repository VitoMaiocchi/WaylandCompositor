CXX = clang++
CC = clang

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=-L/usr/lib \
	 $(shell pkg-config --cflags --libs "wlroots-0.18") \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon)

FLAGS=$(CFLAGS) -Wall -I/usr/include/pixman-1 -I. -I/usr/include/wlroots-0.18 -DWLR_USE_UNSTABLE

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		wlr-layer-shell-unstable-v1.xml $@

#wayland.o: wayland.c layout.h xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h surface2.h
#	$(CC) -c $< $(FLAGS)

wayland.o: wayland.cpp xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h surface2.h
	$(CXX) -c $< $(FLAGS)

#layout.o: layout.c layout.h wayland.h config.h
#	$(CC) -c $< $(FLAGS)

surface.o: surface.cpp surface.h wayland.h config.h
	$(CXX) -c $< $(FLAGS)

VitoWM: wayland.o surface.o
	$(CXX) -o VitoWM $^ $(LIBS)

clean:
	rm -f tinywl VitoWM xdg-shell-protocol.h xdg-shell-protocol.c wayland.o layout.o surface.o

.DEFAULT_GOAL=VitoWM
.PHONY: clean
