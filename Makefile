CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
VLISPDIR = $(HOME)/.vlisp

all: poco lisp vlisp vbasic vminimo tiny_scheme

poco: src/poco.cpp src/poco.h
	$(CXX) $(CXXFLAGS) src/poco.cpp -o poco

lisp: src/lisp.cpp src/lisp.h
	$(CXX) $(CXXFLAGS) src/lisp.cpp -o lisp

vlisp: src/vlisp.cpp src/vlisp.h
	$(CXX) $(CXXFLAGS) src/vlisp.cpp -o vl

vbasic: src/vbasic.cpp 
	$(CXX) $(CXXFLAGS) src/vbasic.cpp -o vbasic

vminimo: src/vminimo.cpp 
	$(CXX) $(CXXFLAGS) src/vminimo.cpp -o vminimo

tiny_scheme: src/tiny_scheme.cpp 
	$(CXX) $(CXXFLAGS) src/tiny_scheme.cpp -o tiny_scheme


install: vlisp
	install -d $(BINDIR) $(VLISPDIR)
	install -m 755 vl $(BINDIR)/vl
	install -m 644 src/prelude.lisp $(VLISPDIR)/prelude.lisp

uninstall: vlisp
	rm -f $(BINDIR)/vl
	rm -rf $(VLISPDIR)

clean:
	rm -f poco lisp vlisp vbasic vminimo

.PHONY: all clean 

