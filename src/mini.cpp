#include "mini.h"
#include <fstream>
// usage: mini [file ...]   files are evaluated in order; with no file, a REPL
void report(const Error& e) {
    std::cerr << "error: " << e.what();
    if (e.node) print(e.node, std::cerr << "\n    in ");
    std::cerr << '\n';
}
int main(int argc, char** argv) {
    EnvPtr env = globals();
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i]);
        if (!f) { std::cerr << "cannot open " << argv[i] << '\n'; return 1; }
        try { for (Ptr x; (x = read(f));) eval(x, env); } catch (Error& e) { report(e); return 1; }
    }
    while (argc == 1 && std::cout << ">> " << std::flush) {
        try {
            Ptr x = read(std::cin);
            if (!x) break;
            print(eval(x, env), std::cout) << '\n';
        } catch (Error& e) {
            report(e);
            std::cin.ignore(1 << 30, '\n');                   // the rest of the line is dropped
        }
    }
}
