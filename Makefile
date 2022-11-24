# Install
BIN = demo

# Flags
CFLAGS += -std=c99 -Wall -Wextra -O2
SDLFLAGS = $(shell pkg-config --cflags SDL2) $(shell pkg-config --libs SDL2)
CGLMFLAGS += $(shell pkg-config --cflags cglm) $(shell pkg-config --libs cglm)

SRC = main.c
OBJ = $(SRC:.c=.o)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LIBS = -lSDL2 -framework OpenGLES -lm $(SDLFLAGS)

else
	LIBS = -lSDL2 -lGLESv2 -lm
endif

$(BIN): prepare
	$(CC) $(SRC) $(CFLAGS) -g -o bin/$(BIN) $(LIBS) $(SDL_FLAGS) $(CGLMFLAGS)

web: prepare
	emcc $(SRC) -Os -s USE_SDL=2 -o bin/index.html

rpi: prepare
	$(CC) $(SRC) $(CFLAGS) -o bin/$(BIN) `PKG_CONFIG_PATH=/opt/vc/lib/pkgconfig/ shell pkg-config --cflags --libs bcm_host brcmglesv2` `/usr/local/bin/sdl2-config --libs --cflags`

prepare:
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)

clean:
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)

.PHONY: check-syntax all

check-syntax:
	$(CC) $(CFLAGS) -Wall -Wextra -fsyntax-only $(SRC) $(LIBS) $(SDL_FLAGS)

all: $(BIN)
