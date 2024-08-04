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


wayland.o: wayland.cpp xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h surface.hpp
	$(CXX) -c $< $(FLAGS)

layout.o: layout.cpp layout.hpp
	$(CXX) -c $< $(FLAGS)

surface.o: surface.cpp surface.hpp wayland.hpp config.hpp
	$(CXX) -c $< $(FLAGS)

VitoWM: wayland.o surface.o layout.o
	$(CXX) -o VitoWM $^ $(LIBS)

clean:
	rm -f VitoWM xdg-shell-protocol.h xdg-shell-protocol.c wayland.o layout.o surface.o

.DEFAULT_GOAL=VitoWM
.PHONY: clean
