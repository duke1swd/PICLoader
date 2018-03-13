all: loader hexcrack sample.hex rample.hex

loader: loader.c ../commands.h
	gcc -c -Wall -I.. loader.c
	gcc -o loader loader.o

hexcrack: hexcrack.c
	gcc -Wall -c hexcrack.c
	gcc -o hexcrack hexcrack.o

sample.hex: sample.c
	/cygdrive/c/Program\ Files/bknd/cc5x/cc5x -Ln sample.c

rample.hex: rample.c
	/cygdrive/c/Program\ Files/bknd/cc5x/cc5x -Ln rample.c
