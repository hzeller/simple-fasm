CXXFLAGS=-std=c++17 -W -Wall -Wextra -O3
CXX=g++

all: fasm-parse_test

test: fasm-parse_test
	./fasm-parse_test

fasm-parse_test.o: fasm-parse.h

% : %.o
	$(CXX) -o $@ $^

clean:
	rm -f *.o fasm-parse_test
