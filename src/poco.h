// poco.h
//

#ifndef POCO_H
#define POCO_H

#include <iostream>
#include <string>

void repl () {
	while (true) {
		std::cout << ">> ";
		std::string line;
		std::getline (std::cin, line);
	}
}
#endif // POCO_H

// eof

