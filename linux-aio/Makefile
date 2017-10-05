CXX=g++
CXXFLAGS=-std=c++11 -lpthread -Wl,--no-as-needed
CC=gcc
LDFLAGS=-laio -fopenmp

FILES = test-all test-one-write test-syncio test-mp-rand-read #test1

all: $(FILES)

test-one-write: test-one-write.o
	$(CC) -o test-one-write test-one-write.o $(LDFLAGS)

test-all: test-all.o
	$(CXX) $(CXXFLAGS) -o test-all test-all.o $(LDFLAGS)

test-syncio: test-syncio.o
	$(CXX) $(CXXFLAGS) -o test-syncio test-syncio.o $(LDFLAGS)

test-mp-rand-read: test-mp-rand-read.o
	$(CXX) $(CXXFLAGS) -o test-mp-rand-read test-mp-rand-read.o $(LDFLAGS)

test1: test1.o
	$(CXX) $(CXXFLAGS) -o test1 test1.o $(LDFLAGS)

clean:
	rm -f $(FILES)
	rm -f *.o
