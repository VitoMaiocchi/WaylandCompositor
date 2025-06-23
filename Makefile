CXX = clang++
CC = clang

CXX_STANDARD = c++23

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=-L/usr/lib \
	 $(shell pkg-config --cflags --libs "wlroots-0.18") \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon) \
	 $(shell pkg-config --cflags --libs cairo) \
	 $(shell pkg-config --cflags --libs xcb) \
	 $(shell pkg-config --cflags --libs libpulse) \
	 $(shell pkg-config --cflags --libs libspa-0.2 libpipewire-0.3 wireplumber-0.5)

FLAGS=$(CFLAGS) -g -O0 -Wall -I/usr/include/pixman-1 -I. -I./include -I/usr/include/wlroots-0.18 -DWLR_USE_UNSTABLE \
	$(shell pkg-config --cflags libspa-0.2 libpipewire-0.3 wireplumber-0.5)

GREEN=\033[1;32m
RED=\033[1;31m
BLUE=\033[1;34m
YELLOW=\033[1;33m
END=\033[0m

#generate C headers for Wayland Protocols
include/xdg-shell-protocol.h:
	@echo -e "$(BLUE)Generating XDG shell protocol...$(END)"
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

include/wlr-layer-shell-unstable-v1-protocol.h:
	@echo -e "$(BLUE)Generating LayerShell protocol...$(END)"
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@

include/wlr-screencopy-unstable-v1-protocol.h:
	@echo -e "$(BLUE)Generating LayerShell protocol...$(END)"
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-screencopy-unstable-v1.xml $@


SRCS := $(shell find src -name '*.cpp')
OBJS = $(patsubst src/%.cpp, build/%.o, $(SRCS))
HEADERS = $(wildcard include/*.hpp)

build/%.o: src/%.cpp $(HEADERS) include/xdg-shell-protocol.h include/wlr-layer-shell-unstable-v1-protocol.h include/wlr-screencopy-unstable-v1-protocol.h
	@mkdir -p build
	@mkdir -p $(dir $@)
	@echo -e "$(YELLOW)Compiling $<...$(END)"
	$(CXX) -c $< -o $@ $(FLAGS) -std=$(CXX_STANDARD)

VitoWM: $(OBJS)
	@echo -e "$(GREEN)Linking Final Executable...$(END)"
	$(CXX) -o VitoWM $^ $(LIBS) -std=$(CXX_STANDARD)

wpctl: src/wpctl.c
	$(CC) -o wpctl src/wpctl.c $(shell pkg-config --cflags --libs libspa-0.2 libpipewire-0.3 wireplumber-0.5)

clean:
	rm -r VitoWM include/xdg-shell-protocol.h include/wlr-layer-shell-unstable-v1-protocol.h include/wlr-screencopy-unstable-v1-protocol.h build gdb_stacktrace.txt

.DEFAULT_GOAL=VitoWM
.PHONY: clean
