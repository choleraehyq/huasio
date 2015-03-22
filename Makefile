.PHONY : all
all : huio.o demo
huio.o : huio.cc
	g++ -std=c++11 huio.cc -c -o huio.o -lpthread -g
demo : demo.cc huio.o 
	g++ -std=c++11 demo.cc -o demo huio.o -lpthread -g

.PHONY : clean
clean :
	-rm demo huio.o
	-rm *~
