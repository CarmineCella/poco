#include "vlisp.h"
// usage: vl [file ...] [-]   files are loaded in order (a prelude, then a script); "-" starts the REPL
int main(int argc, char** argv) {
    EnvPtr env = globals();
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-") { repl(env); continue; }
        try { load({cell(std::string(argv[i]))}, env); } catch (Error& e) { report(e, std::cerr); return 1; }
    }
    if (argc == 1) repl(env);
}
