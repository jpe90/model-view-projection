# Install
BIN = main

# Flags
CFLAGS += -std=c99 -Wall -Wextra  -Wno-unused-variable -Wno-unused-function -g
SDL2FLAGS = $(shell pkg-config --libs SDL2) $(shell pkg-config --cflags SDL2)

SRC = main.c
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
BIN := $(BIN).exe
LIBS = -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lm -lGLU32 -lGLEW32
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		LIBS = -lSDL2 -framework OpenGL -lm -lGLEW $(SDL2FLAGS)
	else
		LIBS = -lSDL2 -lGL -lm -lGLU -lGLEW
	endif
endif

$(BIN):
	@mkdir -p bin
	rm -f bin/$(BIN) $(OBJS)
	$(CC) $(SRC) $(CFLAGS) -o bin/$(BIN) $(LIBS)

all: $(BIN)
