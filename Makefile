FLAGS = -g -O3 -Wall
LIBS = -I/usr/include/freetype2 -lX11 -lfreetype
CFILES = arena.c config.c directory.c font.c gui.c main.c utils.c

make:
	gcc ${FLAGS} ${CFILES} ${LIBS} -o pistachio
