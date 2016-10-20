all: midtermsh

midtermsh: midtermsh.o
	g++ -o midtermsh midtermsh.o

midtermsh.o: midtermsh.c
	g++ -std=c++11 -D_POSIX_C_SOURCE -o midtermsh.o -c midtermsh.c

clean: 
	rm *.o midtermsh
