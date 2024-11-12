CXX = clang++
CC = clang

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=-L/usr/lib \
	 $(shell pkg-config --cflags --libs "wlroots-0.18") \
	 $(shell pkg-config --cflags --libs wayland-server) \
	 $(shell pkg-config --cflags --libs xkbcommon) \
	 $(shell pkg-config --cflags --libs cairo)

FLAGS=$(CFLAGS) -g -O0 -Wall -I/usr/include/pixman-1 -I. -I/usr/include/wlroots-0.18 -DWLR_USE_UNSTABLE

GREEN=\033[1;32m
RED=\033[1;31m
BLUE=\033[1;34m
YELLOW=\033[1;33m
END=\033[0m

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	@echo -e "$(BLUE)Generating XDG shell protocol...$(END)"
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

wlr-layer-shell-unstable-v1-protocol.h:
	@echo -e "$(BLUE)Generating LayerShell protocol...$(END)"
	$(WAYLAND_SCANNER) server-header \
		wlr-layer-shell-unstable-v1.xml $@


SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
HEADERS = $(wildcard *.hpp)

%.o: %.cpp $(HEADERS) xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h
	@mkdir -p build
	@echo -e "$(YELLOW)Compiling $<...$(END)"
	$(CXX) -c $< $(FLAGS)

VitoWM: $(OBJS)
	@echo -e "$(GREEN)Linking Final Executable...$(END)"
	$(CXX) -o VitoWM $^ $(LIBS)

clean:
	rm -f VitoWM xdg-shell-protocol.h xdg-shell-protocol.c *.o

.DEFAULT_GOAL=VitoWM
.PHONY: clean
