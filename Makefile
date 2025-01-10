TARGET_NAME=rosewm
BUILD_DIR=build

WAYLAND_PROTOCOLS_DIR =\
 $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER =\
 $(shell pkg-config --variable=wayland_scanner wayland-scanner)

CFLAGS =\
 -Wall -Wextra -O2 -std=c2x -Isrc/ \
 -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L \
 $(shell pkg-config --cflags wlroots-0.18) \
 $(shell pkg-config --cflags wayland-server) \
 $(shell pkg-config --cflags libinput) \
 $(shell pkg-config --cflags xkbcommon) \
 $(shell pkg-config --cflags pixman-1) \
 $(shell pkg-config --cflags freetype2) \
 $(shell pkg-config --cflags fribidi)

CLIBS =\
 -lm\
 $(shell pkg-config --libs wlroots-0.18) \
 $(shell pkg-config --libs wayland-server) \
 $(shell pkg-config --libs libinput) \
 $(shell pkg-config --libs xkbcommon) \
 $(shell pkg-config --libs pixman-1) \
 $(shell pkg-config --libs freetype2) \
 $(shell pkg-config --libs fribidi)

SRC = $(sort $(wildcard src/*.c))
DEPS_OBJ = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRC))
DEPS_SRC =\
 src/pointer-constraints-unstable-v1-protocol.c \
 src/tablet-v2-protocol.c \
 src/xdg-shell-protocol.c

obtain_object_files = $(patsubst $(BUILD_DIR)/%.c,-l:%.o,$(1))

program: $(DEPS_OBJ)
	$(CC) $(CFLAGS) $(call obtain_object_files,$^) \
		$(CLIBS) -o $(BUILD_DIR)/$(TARGET_NAME)

clean:
	rm -f $(BUILD_DIR)/$(TARGET_NAME)
	rm -f $(BUILD_DIR)/*.o

install:
	cp $(BUILD_DIR)/$(TARGET_NAME) /usr/local/bin/$(TARGET_NAME)

uninstall:
	rm /usr/local/bin/$(TARGET_NAME)

protocols: $(DEPS_SRC)

protocols_clean:
	rm -f src/pointer-constraints-unstable-v1-protocol.c
	rm -f src/tablet-v2-protocol.c
	rm -f src/xdg-shell-protocol.c
	rm -f src/pointer-constraints-unstable-v1-protocol.h
	rm -f src/tablet-v2-protocol.h
	rm -f src/xdg-shell-protocol.h

src/pointer-constraints-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS_DIR)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@

src/pointer-constraints-unstable-v1-protocol.c: src/pointer-constraints-unstable-v1-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS_DIR)/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml $@

src/tablet-v2-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS_DIR)/stable/tablet/tablet-v2.xml $@

src/tablet-v2-protocol.c: src/tablet-v2-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS_DIR)/stable/tablet/tablet-v2.xml $@

src/xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell-protocol.c: src/xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml $@

$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
