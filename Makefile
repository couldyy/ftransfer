CC ?= gcc
CC_WIN ?= x86_64-w64-mingw32-gcc


all: ftransfer

ftransfer: ftransfer.c utils.h utils.c
	$(CC) -o ftransfer ftransfer.c utils.c

windows:
	$(CC_WIN) -o ftransfer.exe ftransfer.c utils.c -lws2_32
