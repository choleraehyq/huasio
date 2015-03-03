.PHONY : all
all : huasio.o demo
huasio.o : huasio.cc
	g++ -std=c++11 huasio.cc -c -o huasio.o -lpthread -g
demo : demo.cc huasio.o 
	g++ -std=c++11 demo.cc -o demo -lpthread huasio.o -g

.PHONY : clean
clean :
	-rm demo huasio.o
	-rm *~
