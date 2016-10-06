all : rshd

.PHONY : all clean

rshd : main.o
	g++ -std=c++11 -o $@ $<

main.o : main.cpp
	g++ -std=c++11  -o $@ -c $<

clean :
	rm -rf *.o rshd

