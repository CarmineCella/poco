#include "lisp.h"
#include <fstream>

int main(int argc, char** argv) {
    EnvPtr env = globals();
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i]);
        try {
            run(file, env, false);
        } catch (std::exception& e) {
            std::cerr << argv[i] << ": " << e.what() << '\n';
            return 1;
        }
    }
    if (argc == 1) repl(env);
}
