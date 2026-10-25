// vlisp.cpp
//

#include "vlisp.h"

using namespace std;

// usage: vl [file ...] [-] files are loaded in order; "-" starts the REPL
int main(int argc, char** argv) {
    EnvPtr env = globals();
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-") {
            repl(env);
            continue;
        }
        try {
            load({cell(std::string(argv[i]))}, env);
        } catch (Error& e) {
            report(e, std::cerr);
            return 1;
        }
    }
    if (argc == 1) {
        cout << "[vlisp, v0.1]" << endl << endl;
        repl(env);
    }

    return 0;
}

// eof
