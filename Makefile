CXX = g++
CPPFLAGS = -Isrc
CXXFLAGS = -Wall -W -pedantic -std=gnu++11 -O2
LIBFLAGS = -Llib -lpz
TARGETS = lib/libpz.a bin/pzip
INSTALL_PATH = /usr/local
SOURCES = src/libpz.cc
OBJECTS = src/libpz.o

.PHONY: all clean install test library

all: $(TARGETS)

clean:
	rm -f test.txt test.txt.pz src/*.o $(TARGETS)
	rm -rf bin lib

install:
	install -m 644 lib/libpz.a $(INSTALL_PATH)/lib
	install -m 644 src/pzip.hh $(INSTALL_PATH)/include
	install -m 755 bin/pzip $(INSTALL_PATH)/bin

test: $(TARGETS)
	cp README test.txt
	./bin/pzip test.txt
	sha256sum test.txt.pz

library: lib/libpz.a

lib/libpz.a: $(OBJECTS)
	if [ ! -d lib ]; then mkdir -vp lib; fi
	ar crfv $@ $^ 

bin/pzip: src/pzip.o lib/libpz.a
	if [ ! -d bin ]; then mkdir -vp bin; fi
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBFLAGS)