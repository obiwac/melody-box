#!/bin/sh
set -e

if [ ! -f "stb_image.o" ]; then
	cc -c stb_image.c -o stb_image.o
fi

cc -lgpio -lm sh1106.c stb_image.o -o sh1106
./sh1106
