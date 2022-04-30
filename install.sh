#!/usr/bin/bash

CAN_BUILD=1
if [ ! -d /usr/include/freetype2 ]; then
	echo "Could not locate freetype2 headers (/usr/include/freetype2)"
	CAN_BUILD=0
fi
if [ ! -f /usr/lib/libfreetype.so ]; then
	echo "Could not locate freetype2 library (/usr/lib/libfreetype.so)"
	CAN_BUILD=0
fi
if [ ! -f /usr/lib/libX11.so ]; then
	echo "Could not locate X11 library (/usr/lib/libX11.so)"
	CAN_BUILD=0
fi

(( CAN_BUILD == 0 )) && exit

FONT="/usr/share/fonts/noto/NotoSansMono-Regular.ttf"
if [ -f /usr/bin/fc-match ]; then
	FONT=`fc-match monospace -f "%{file}"`
fi

FLAGS="-O3 -Wall"
SOURCES="arena.c config.c directory.c font.c gui.c main.c utils.c"

echo "Compiliing..."
gcc ${FLAGS} -DFONT_PATH=\"$FONT\" ${SOURCES} -I/usr/include/freetype2 -lX11 -lfreetype -o pistachio

(( $? != 0 )) && exit

echo "Installing..."
sudo cp pistachio /usr/bin

if (( $? == 0 )); then
	echo "Done!"
	echo "You may wish to create a keyboard shortcut to /usr/bin/pistachio to quickly start the program"
fi

