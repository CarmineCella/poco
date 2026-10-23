CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(HOME)/.poco

all: poco

poco: src/poco.cpp src/poco.h
	$(CXX) $(CXXFLAGS) src/poco.cpp -o poco

install: poco
	install -d $(BINDIR) $(LIBDIR)
	install -m 755 poco $(BINDIR)/poco
	install -m 644 src/stdlib.poco $(LIBDIR)/stdlib.poco

uninstall: poco
	rm -f $(BINDIR)/poco
	rm -rf $(LIBDIR)	


clean:
	rm -f poco

.PHONY: all clean install uninstall

