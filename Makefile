CXX = g++
CXXFLAGS = -std=c++14 -O2 -Wall -Wextra -Iinclude

all: pkg-resolver test_suite

pkg-resolver: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o pkg-resolver

test_suite: tests/test_resolver.cpp
	$(CXX) $(CXXFLAGS) tests/test_resolver.cpp -o test_suite

test: test_suite
	./test_suite

clean:
	rm -f pkg-resolver pkg-resolver.exe test_suite test_suite.exe *.o

.PHONY: all test clean
