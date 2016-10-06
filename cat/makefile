all: cat

cat: main.o
	gcc -o cat main.o

main.o: main.c
	gcc -std=c99 -o main.o -c main.c

clear: 
	rm *.o cat
