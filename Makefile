CXXFLAGS=-std=c++17 -W -Wall -Wextra -pedantic -O3
CXX=g++

BINARIES=fasm-parse_test fasm-validation-parse fasm-generate-testfile

all: $(BINARIES)

test: fasm-parse_test
	./fasm-parse_test

fasm-parse_test.o: fasm-parse.h
fasm-validation-parse.o: fasm-parse.h

% : %.o
	$(CXX) -o $@ $^

# Add these so that we have tab-completion in make
$(BINARIES):

clean:
	rm -f *.o $(BINARIES)
