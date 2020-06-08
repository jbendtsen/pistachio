make:
	gcc -g -O3 -I/usr/include/freetype2 -lX11 -lfreetype -lfontconfig *.c -o pistachio
