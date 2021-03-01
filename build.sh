#!/bin/bash

CC=
if command -v clang 2>&1 > /dev/null; then
    CC=clang
elif command -v gcc 2>&1 > /dev/null; then
    CC=gcc
else
    echo 'Neither gcc nor clang found! Must have either gcc or clang to compile.'
    exit 1
fi


case "$1" in
	debug)
		if ! $CC -g unix.c -o aseprite_ssd1306; then
			exit 1
		fi
		;;
	'')
		if ! $CC -DRELEASE=1 -O3 unix.c -o aseprite_ssd1306; then
			exit 1
		fi
		;;
esac	
