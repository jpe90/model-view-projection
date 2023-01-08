# Install
BIN = main

# Flags
CFLAGS += -std=c99 -Wall -Wextra -O2
SDLFLAGS = $(shell pkg-config --cflags sdl2) $(shell pkg-config --libs sdl2)

SRC = main.c
OBJ = $(SRC:.c=.o)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	LIBS = -framework OpenGLES -lm $(SDLFLAGS) $(CGLMFLAGS)
else
	LIBS = -lGLESv2 -lm $(SDLFLAGS) $(CGLMFLAGS)
endif

$(BIN): $(SRC)
	$(CC) $(SRC) $(CFLAGS) -g -o $(BIN) $(LIBS)

clean:
	rm $(OBJS)

.PHONY: check-syntax all

check-syntax:
	$(CC) $(CFLAGS) -Wall -Wextra -fsyntax-only $(SRC) $(LIBS) $(SDL_FLAGS)

all: $(BIN)
