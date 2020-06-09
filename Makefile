FLAGS = -g -O3
LIBS = -I/usr/include/freetype2 -lX11 -lfreetype -lfontconfig
CFILES = directory.c font.c gui.c main.c utils.c

make:
	gcc ${FLAGS} ${LIBS} ${CFILES} -o pistachio
