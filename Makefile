CC = gcc
CFLAGS = -I/mingw64/include
LDFLAGS = -L/mingw64/lib -lraylib -lopengl32 -lgdi32 -lwinmm
SRC = src/main.c src/snake.c
OUT = snake_game.exe

all:
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS)
