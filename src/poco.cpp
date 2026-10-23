#include "poco.h"
#include <fstream>

int main(int argc, char** argv) {
    if (argc == 1) {
        repl();
        return 0;
    }
    if (argc != 2) {
        std::cerr << "usage: poco [file.poco]\n";
        return 1;
    }
    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "cannot open file: " << argv[1] << '\n';
        return 1;
    }
    try {
        OpTable ops = make_op_table();
        Environment env;
        ProcTable procs;
        NodePtr program = read(input, ops);
        exec(as<Block>(program), env, procs);
    } catch (const PocoError& e) {
        show_error(e);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
