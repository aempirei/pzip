CXX = g++
CPPFLAGS = -Isrc
CXXFLAGS = -Wall -W -pedantic -std=gnu++1y -O2
LIBFLAGS = -Llib -lpz
TARGETS = lib/libpz.a bin/pzip bin/qzip bin/rzip bin/esl
INSTALL_PATH = /usr/local

.PHONY: all clean install test library

all: $(TARGETS)

clean:
	rm -f test.txt test.txt.pz src/*.o $(TARGETS)
	rm -rf bin lib

install: $(TARGETS)
	install -m 644 lib/libpz.a $(INSTALL_PATH)/lib
	install -m 644 src/libpz.hh $(INSTALL_PATH)/include
	install -m 755 bin/pzip $(INSTALL_PATH)/bin

test: $(TARGETS)
	cp README test.txt
	./bin/pzip test.txt
	sha256sum test.txt.pz

library: lib/libpz.a

lib/libpz.a: src/libpz.o
	if [ ! -d lib ]; then mkdir -vp lib; fi
	ar crfv $@ $^ 

bin/pzip: src/pzip.o src/config.o lib/libpz.a
	if [ ! -d bin ]; then mkdir -vp bin; fi
	$(CXX) $(CXXFLAGS) -o $@ $+

bin/qzip: src/qzip.o src/config.o
	if [ ! -d bin ]; then mkdir -vp bin; fi
	$(CXX) $(CXXFLAGS) -o $@ $+

bin/rzip: src/rzip.o src/config.o
	if [ ! -d bin ]; then mkdir -vp bin; fi
	$(CXX) $(CXXFLAGS) -o $@ $+

bin/esl: src/esl.o
	if [ ! -d bin ]; then mkdir -vp bin; fi
	$(CXX) $(CXXFLAGS) -o $@ $<
