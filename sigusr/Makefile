all: sigusr

sigusr: sigusr.o
	gcc -o sigusr sigusr.o

sigusr.o: sigusr.c
	gcc -o sigusr.o -c sigusr.c

clean: 
	rm *.o sigusr
