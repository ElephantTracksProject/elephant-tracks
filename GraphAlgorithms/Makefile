include ../Makefile.inc

all: libGraphAlgorithms.so 

#GraphAlgorithms.o: GraphAlgorithms.h
#	g++ $(CXXFLAGS) GraphAlgorithms.cpp -c -I. -o GraphAlgorithms.o

HashGraph.o: HashGraph.cpp HashGraph.h
	g++ $(CXXFLAGS) HashGraph.cpp -c -I. -o HashGraph.o

libGraphAlgorithms.so: HashGraph.o GraphAlgorithms.h
	g++ $(CXXFLAGS) -shared HashGraph.o -o libGraphAlgorithms.so

testGraphAlgorithms: libGraphAlgorithms.so GraphAlgorithms.o HashGraph.o testGraphAlgorithms.cpp
	g++ $(CXXFLAGS) -L. -I. -lGraphAlgorithms -lHashGraph testGraphAlgorithms.cpp -o testGraphAlgorithms

clean:
	rm -f *.o *.so *.a
