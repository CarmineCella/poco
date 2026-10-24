CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(HOME)/.poco

all: poco lisp vlisp vbasic vminimo

poco: src/poco.cpp src/poco.h
	$(CXX) $(CXXFLAGS) src/poco.cpp -o poco

lisp: src/lisp.cpp src/lisp.h
	$(CXX) $(CXXFLAGS) src/lisp.cpp -o lisp

vlisp: src/vlisp.cpp src/vlisp.h
	$(CXX) $(CXXFLAGS) src/vlisp.cpp -o vlisp

vbasic: src/vbasic.cpp 
	$(CXX) $(CXXFLAGS) src/vbasic.cpp -o vbasic

vbasic: src/vminimo.cpp 
	$(CXX) $(CXXFLAGS) src/vminimo.cpp -o vminimo

clean:
	rm -f poco lisp vlisp vbasic vminimo

.PHONY: all clean 

