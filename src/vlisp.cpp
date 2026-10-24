#include "vlisp.h"
#include <fstream>

int main(int argc, char** argv) {
    EnvPtr env = globals();
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i]);
        try {
            run(file, env, false);
        } catch (Error& e) {
            std::cerr << argv[i] << ": ";
            report(e, std::cerr);
            return 1;
        }
    }
    if (argc == 1) {
        try {
            repl(env);
        } catch (Error& e) {
            report(e, std::cerr);
            return 1;
        }
    }
}
