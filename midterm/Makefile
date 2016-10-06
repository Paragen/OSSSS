all: midtermsh

midtermsh: midtermsh.o
	gcc -o midtermsh midtermsh.o

midtermsh.o: midtermsh.c
	gcc -std=c99 -D_POSIX_C_SOURCE -o midtermsh.o -c midtermsh.c

clean: 
	rm *.o midtermsh
