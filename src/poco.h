#ifndef POCO_H
#define POCO_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

// --- Data -----------------------------------------------------------------
using Array = std::valarray<double>;
struct Node;
using NodePtr = std::shared_ptr<Node>;
using Block = std::vector<NodePtr>;
using Opcode = Block (*)(const Block&);
using OpTable = std::unordered_map<std::string, Opcode>;
using Environment = std::unordered_map<std::string, NodePtr>;
using ProcTable = std::unordered_map<std::string, Block>;

struct Sym { std::string name; };
using Value = std::variant<Block, Array, Sym, std::string, Opcode>;
struct Node : Value {
    using Value::Value;
    std::size_t line = 0;           // Only set for instruction nodes
    std::string source;             // Original instruction line, for errors
};

template<class T>
inline NodePtr make(T value) { return std::make_shared<Node>(std::move(value)); }
template<class T>
inline bool is(const NodePtr& p) { return std::holds_alternative<T>(*p); }
template<class T>
inline T& as(const NodePtr& p) { return std::get<T>(*p); }

struct PocoError : std::runtime_error {
    using std::runtime_error::runtime_error;
    std::size_t line = 0;
    std::string source;
};

[[noreturn]] inline void error(const std::string& message) {
    throw PocoError(message);
}

// --- Scanner and reader ---------------------------------------------------
inline std::string scan(std::istream& input) {
    std::string token;
    while (true) {
        int ch = input.get();
        if (ch == EOF) return token;
        char c = static_cast<char>(ch);
        if (c == '{' || c == '}' || c == '\n') {
            if (!token.empty()) {
                input.unget();
                return token;
            }
            return std::string(1, c);
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) return token;
            continue;
        }
        if (c == '#') {
            if (!token.empty()) {
                input.unget();
                return token;
            }
            while (input.peek() != EOF && input.peek() != '\n') input.get();
            continue;
        }
        if (c == '"') {
            if (!token.empty()) {
                input.unget();
                return token;
            }
            token = "\"";
            while (true) {
                ch = input.get();
                if (ch == EOF || ch == '\n') error("string not closed");
                c = static_cast<char>(ch);
                token += c;
                if (c == '"') return token;
            }
        }
        token += c;
    }
}

inline bool is_number(const std::string& token) {
    std::stringstream stream(token);
    double value;
    stream >> value;
    return stream && stream.eof();
}
inline bool is_string(const std::string& token) {
    return token.size() >= 2 && token.front() == '"' && token.back() == '"';
}

inline NodePtr read_body(std::istream& input, const OpTable& ops,
                         std::size_t& line,
                         const std::vector<std::string>& lines,
                         bool nested = false) {
    Block body, cmd;
    std::size_t command_line = 0;

    auto finish_command = [&]() {
        if (cmd.empty()) return;
        NodePtr instruction = make(std::move(cmd));
        instruction->line = command_line;
        if (command_line > 0 && command_line <= lines.size())
            instruction->source = lines[command_line - 1];
        body.push_back(instruction);
        cmd = {};
    };

    while (true) {
        std::string token = scan(input);
        if (token.empty()) {
            if (nested) error("block not closed");
            finish_command();
            return make(std::move(body));
        }
        if (token == "\n" || token == "}") {
            finish_command();
            if (token == "}") {
                if (!nested) error("unexpected }");
                return make(std::move(body));
            }
            ++line;
            continue;
        }
        if (cmd.empty()) command_line = line;
        if (token == "{") {
            cmd.push_back(read_body(input, ops, line, lines, true));
        } else if (is_number(token)) {
            cmd.push_back(make(Array{std::stod(token)}));
        } else if (is_string(token)) {
            cmd.push_back(make(token.substr(1, token.size() - 2)));
        } else if (auto it = ops.find(token); it != ops.end()) {
            cmd.push_back(make(Opcode{it->second}));
        } else {
            cmd.push_back(make(Sym{token}));
        }
    }
}

inline NodePtr read(std::istream& input, const OpTable& ops) {
    // read() already consumes the whole program. Keeping the text also
    // allows accurate source lines in errors, including procedure bodies.
    std::string source(std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{});
    std::vector<std::string> lines;
    std::istringstream line_input(source);
    std::string text;
    while (std::getline(line_input, text)) lines.push_back(text);
    std::istringstream parser(source);
    std::size_t line = 1;
    try {
        return read_body(parser, ops, line, lines);
    } catch (PocoError& e) {
        if (!e.line) {
            e.line = std::min(line, std::max<std::size_t>(1, lines.size()));
            if (e.line <= lines.size()) e.source = lines[e.line - 1];
        }
        throw;
    } catch (const std::exception& e) {
        PocoError p(e.what());
        p.line = std::min(line, std::max<std::size_t>(1, lines.size()));
        if (p.line <= lines.size()) p.source = lines[p.line - 1];
        throw p;
    }
}

// --- Printing -------------------------------------------------------------
inline std::ostream& print(const NodePtr& n, std::ostream& out) {
    if (!n) return out;
    if (is<Block>(n)) {
        const Block& b = as<Block>(n);
        out << '{';
        for (std::size_t i = 0; i < b.size(); ++i) {
            if (i) out << ' ';
            print(b[i], out);
        }
        out << '}';
    } else if (is<Array>(n)) {
        const Array& a = as<Array>(n);
        out << '[';
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (i) out << ' ';
            out << a[i];
        }
        out << ']';
    } else if (is<Sym>(n)) {
        out << as<Sym>(n).name;
    } else if (is<std::string>(n)) {
        out << as<std::string>(n);
    } else if (is<Opcode>(n)) {
        out << "<opcode>";
    }
    return out;
}

// --- Evaluation -----------------------------------------------------------
inline Block eval(const Block& cmd, Environment& env, ProcTable& procs);

inline Block exec(const Block& body, Environment& env, ProcTable& procs) {
    Block result;
    for (const auto& instruction : body) {
        try {
            result = eval(as<Block>(instruction), env, procs);
        } catch (PocoError& e) {
            if (!e.line) {
                e.line = instruction->line;
                e.source = instruction->source;
            }
            throw;
        } catch (const std::exception& e) {
            PocoError p(e.what());
            p.line = instruction->line;
            p.source = instruction->source;
            throw p;
        }
    }
    return result;
}

inline Block call(const Block& proc, const Block& args, ProcTable& procs) {
    if (proc.size() < 3 || args.size() != proc.size() - 3)
        error("wrong number of arguments");
    Environment local;
    for (std::size_t i = 0; i < args.size(); ++i)
        local[as<Sym>(proc[i + 2]).name] = args[i];
    return exec(as<Block>(proc.back()), local, procs);
}

inline Block eval_if(const Block& cmd, Environment& env, ProcTable& procs) {
    if ((cmd.size() != 3 && cmd.size() != 5) || !is<Block>(cmd[2]))
        error("invalid if");
    if (cmd.size() == 5 &&
        (!is<Sym>(cmd[3]) || as<Sym>(cmd[3]).name != "else" || !is<Block>(cmd[4])))
        error("invalid else");

    NodePtr condition = cmd[1];
    if (is<Sym>(condition)) {
        auto it = env.find(as<Sym>(condition).name);
        if (it == env.end()) error("undefined condition");
        condition = it->second;
    }
    if (!is<Array>(condition) || as<Array>(condition).size() != 1)
        error("invalid condition");
    if (as<Array>(condition)[0] != 0)
        return exec(as<Block>(cmd[2]), env, procs);
    if (cmd.size() == 5)
        return exec(as<Block>(cmd[4]), env, procs);
    return {};
}

inline Block eval_for(const Block& cmd, Environment& env, ProcTable& procs) {
    if (cmd.size() != 4 || !is<Sym>(cmd[1]) || !is<Block>(cmd[3]))
        error("invalid for");

    NodePtr limit = cmd[2];
    if (is<Sym>(limit)) {
        auto it = env.find(as<Sym>(limit).name);
        if (it == env.end()) error("undefined loop limit");
        limit = it->second;
    }
    if (!is<Array>(limit) || as<Array>(limit).size() != 1)
        error("invalid loop limit");
    double n = as<Array>(limit)[0];
    if (!std::isfinite(n) || n < 0 || std::floor(n) != n ||
        n > std::numeric_limits<int>::max())
        error("invalid loop limit");

    const std::string& name = as<Sym>(cmd[1]).name;
    Block result;
    for (int i = 0; i < static_cast<int>(n); ++i) {
        env[name] = make(Array{static_cast<double>(i)});
        result = exec(as<Block>(cmd[3]), env, procs);
    }
    return result;
}

inline Block eval_function(const Block& cmd, ProcTable& procs) {
    if (cmd.size() < 3 || !is<Sym>(cmd[1]) || !is<Block>(cmd.back()))
        error("invalid function");
    for (std::size_t i = 2; i + 1 < cmd.size(); ++i)
        if (!is<Sym>(cmd[i])) error("invalid parameter");
    procs[as<Sym>(cmd[1]).name] = cmd;
    return {};
}

inline Block eval(const Block& cmd, Environment& env, ProcTable& procs) {
    if (cmd.empty()) return {};
    if (is<Sym>(cmd[0])) {
        const auto& name = as<Sym>(cmd[0]).name;
        if (name == "if")       return eval_if(cmd, env, procs);
        if (name == "for")      return eval_for(cmd, env, procs);
        if (name == "function") return eval_function(cmd, procs);
    }

    Block outputs;
    for (std::size_t i = 0; i < cmd.size(); ++i) {
        bool native = is<Opcode>(cmd[i]);
        bool user = is<Sym>(cmd[i]) && procs.count(as<Sym>(cmd[i]).name);
        if (!native && !user) {
            if (!is<Sym>(cmd[i])) error("invalid syntax");
            outputs.push_back(cmd[i]);
            continue;
        }

        Block args;
        for (std::size_t k = i + 1; k < cmd.size(); ++k) {
            NodePtr value = cmd[k];
            if (is<Sym>(value)) {
                const auto& name = as<Sym>(value).name;
                auto it = env.find(name);
                if (it == env.end()) error("undefined symbol: " + name);
                value = it->second;
            }
            if (is<Block>(value) || is<Opcode>(value))
                error("invalid argument");
            args.push_back(value);
        }
        Block results = native
            ? as<Opcode>(cmd[i])(args)
            : call(procs.at(as<Sym>(cmd[i]).name), args, procs);
        if (!outputs.empty() && outputs.size() != results.size())
            error("wrong number of outputs");
        for (std::size_t k = 0; k < outputs.size(); ++k)
            env[as<Sym>(outputs[k]).name] = results[k];
        return results;
    }
    error("missing operation");
}

// --- Primitive numeric functions -----------------------------------------
inline Block numeric(Array a) { return {make(std::move(a))}; }
inline Block scalar(double x) { return numeric(Array{x}); }
template<class Expr>
inline Block numeric_expr(Expr&& expr) {
    Array result = std::forward<Expr>(expr);
    return numeric(std::move(result));
}

inline const Array& array_arg(const Block& args, std::size_t i) {
    if (i >= args.size() || !is<Array>(args[i])) error("expected numeric array");
    return as<Array>(args[i]);
}
inline const Array& one_array(const Block& args) {
    if (args.size() != 1) error("expected one argument");
    const Array& a = array_arg(args, 0);
    if (a.size() == 0) error("empty array");
    return a;
}

template<class F>
inline Block unary(const Block& args, F fn) {
    if (args.size() != 1) error("expected one argument");
    return numeric_expr(fn(array_arg(args, 0)));
}
template<class F>
inline Block binary(const Block& args, F fn) {
    if (args.size() != 2) error("expected two arguments");
    const Array& a = array_arg(args, 0);
    const Array& b = array_arg(args, 1);
    if (!a.size() || !b.size()) error("empty array");
    if (a.size() == b.size()) return numeric_expr(fn(a, b));
    if (a.size() == 1) return numeric_expr(fn(a[0], b));
    if (b.size() == 1) return numeric_expr(fn(a, b[0]));
    error("incompatible array sizes");
}

#define POCO_UNARY(name, EXPR) \
    inline Block fn_##name(const Block& a) { \
        return unary(a, [](const Array& x) { return (EXPR); }); \
    }
#define POCO_BINARY(name, EXPR) \
    inline Block fn_##name(const Block& a) { \
        return binary(a, [](const auto& x, const auto& y) { return (EXPR); }); \
    }

POCO_BINARY(add, x + y)
POCO_BINARY(sub, x - y)
POCO_BINARY(mul, x * y)
POCO_BINARY(div, x / y)
POCO_BINARY(pow, std::pow(x, y))
POCO_BINARY(atan2, std::atan2(x, y))

POCO_UNARY(neg, -x)
POCO_UNARY(abs, std::abs(x))
POCO_UNARY(sin, std::sin(x))
POCO_UNARY(cos, std::cos(x))
POCO_UNARY(tan, std::tan(x))
POCO_UNARY(asin, std::asin(x))
POCO_UNARY(acos, std::acos(x))
POCO_UNARY(atan, std::atan(x))
POCO_UNARY(sinh, std::sinh(x))
POCO_UNARY(cosh, std::cosh(x))
POCO_UNARY(tanh, std::tanh(x))
POCO_UNARY(exp, std::exp(x))
POCO_UNARY(log, std::log(x))
POCO_UNARY(log10, std::log10(x))
POCO_UNARY(log2, std::log(x) / std::log(2.0))
POCO_UNARY(sqrt, std::sqrt(x))
POCO_UNARY(floor, x.apply([](double v) { return std::floor(v); }))
POCO_UNARY(ceil, x.apply([](double v) { return std::ceil(v); }))
POCO_UNARY(round, x.apply([](double v) { return std::round(v); }))
POCO_UNARY(trunc, x.apply([](double v) { return std::trunc(v); }))

#undef POCO_UNARY
#undef POCO_BINARY

inline Block fn_sum(const Block& a) { return scalar(one_array(a).sum()); }
inline Block fn_min(const Block& a) { return scalar(one_array(a).min()); }
inline Block fn_max(const Block& a) { return scalar(one_array(a).max()); }
inline Block fn_mean(const Block& a) {
    const Array& x = one_array(a);
    return scalar(x.sum() / x.size());
}
inline Block fn_size(const Block& a) {
    if (a.size() != 1) error("expected one argument");
    return scalar(static_cast<double>(array_arg(a, 0).size()));
}
inline Block fn_prod(const Block& a) {
    const Array& x = one_array(a);
    return scalar(std::accumulate(std::begin(x), std::end(x),
                                  1.0, std::multiplies<double>{}));
}
inline Block fn_rms(const Block& a) {
    const Array& x = one_array(a);
    Array squares = x * x;
    return scalar(std::sqrt(squares.sum() / x.size()));
}
inline Block fn_norm(const Block& a) {
    const Array& x = one_array(a);
    Array squares = x * x;
    return scalar(std::sqrt(squares.sum()));
}
inline Block fn_variance(const Block& a) {
    const Array& x = one_array(a);
    Array delta = x - x.sum() / x.size();
    Array squares = delta * delta;
    return scalar(squares.sum() / x.size());
}
inline Block fn_stddev(const Block& a) {
    const Array& x = one_array(a);
    Array delta = x - x.sum() / x.size();
    Array squares = delta * delta;
    return scalar(std::sqrt(squares.sum() / x.size()));
}
inline Block fn_dot(const Block& a) {
    if (a.size() != 2) error("dot expects two arguments");
    const Array& x = array_arg(a, 0);
    const Array& y = array_arg(a, 1);
    if (!x.size() || x.size() != y.size())
        error("dot requires equal nonempty arrays");
    Array products = x * y;
    return scalar(products.sum());
}
inline int integer_arg(const Block& a, std::size_t i) {
    const Array& d = array_arg(a, i);
    if (d.size() != 1 || !std::isfinite(d[0]) ||
        std::trunc(d[0]) != d[0] ||
        d[0] < std::numeric_limits<int>::min() ||
        d[0] > std::numeric_limits<int>::max())
        error("expected integer index/offset");
    return static_cast<int>(d[0]);
}
inline std::size_t vector_index(const Block& a, std::size_t i, std::size_t size) {
    int pos = integer_arg(a, i);
    if (pos < 0 || static_cast<std::size_t>(pos) >= size)
        error("vector index out of range");
    return static_cast<std::size_t>(pos);
}
inline Block fn_shift(const Block& a) {
    if (a.size() != 2) error("shift expects two arguments");
    return numeric(array_arg(a, 0).shift(integer_arg(a, 1)));
}
inline Block fn_cshift(const Block& a) {
    if (a.size() != 2) error("cshift expects two arguments");
    return numeric(array_arg(a, 0).cshift(integer_arg(a, 1)));
}

// --- Interface primitives -------------------------------------------------
inline Block fn_vec(const Block& args) {
    std::vector<double> values;
    values.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        const Array& a = array_arg(args, i);
        if (a.size() != 1) error("vec expects scalar elements");
        values.push_back(a[0]);
    }
    if (values.empty()) return numeric(Array(std::size_t{0}));
    return numeric(Array(values.data(), values.size()));
}
inline Block fn_get(const Block& args) {
    if (args.size() != 2) error("get expects vector and index");
    const Array& a = array_arg(args, 0);
    return scalar(a[vector_index(args, 1, a.size())]);
}
inline Block fn_set(const Block& args) {
    // dest set value     => assignment to a variable, through eval()
    if (args.size() == 1) return {args[0]};
    // set vec index value => in-place modification, no outputs
    if (args.size() == 3) {
        if (!is<Array>(args[0])) error("set expects a numeric vector");
        Array& a = as<Array>(args[0]);
        const Array& value = array_arg(args, 2);
        if (value.size() != 1) error("set expects a scalar value");
        std::size_t pos = vector_index(args, 1, a.size());
        a[pos] = value[0];
        return {};
    }
    error("set expects one or three arguments");
}
inline Block fn_print(const Block& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i) std::cout << ' ';
        print(args[i], std::cout);
    }
    std::cout << '\n';
    return {};
}

inline OpTable make_op_table() {
    OpTable ops;
#define REGISTER(name) ops[#name] = &fn_##name
    REGISTER(add);   REGISTER(sub);   REGISTER(mul);   REGISTER(div);
    REGISTER(pow);   REGISTER(atan2);
    REGISTER(neg);   REGISTER(abs);   REGISTER(sin);   REGISTER(cos);
    REGISTER(tan);   REGISTER(asin);  REGISTER(acos);  REGISTER(atan);
    REGISTER(sinh);  REGISTER(cosh);  REGISTER(tanh);  REGISTER(exp);
    REGISTER(log);   REGISTER(log10); REGISTER(log2); REGISTER(sqrt);
    REGISTER(floor); REGISTER(ceil);  REGISTER(round); REGISTER(trunc);
    REGISTER(sum);   REGISTER(min);   REGISTER(max);   REGISTER(mean);
    REGISTER(size);  REGISTER(prod);  REGISTER(rms);   REGISTER(norm);
    REGISTER(variance); REGISTER(stddev); REGISTER(dot);
    REGISTER(shift); REGISTER(cshift);
    REGISTER(vec); REGISTER(get); REGISTER(set); REGISTER(print);
#undef REGISTER
    return ops;
}

inline void show_error(const PocoError& e, std::ostream& out = std::cerr) {
    out << "error";
    if (e.line) out << " (line " << e.line << ")";
    out << ": " << e.what() << '\n';
    if (!e.source.empty()) out << "  " << e.source << '\n';
}

// Whole-input reader + persistent state, accumulating multi-line blocks.
inline void repl() {
    OpTable ops = make_op_table();
    Environment env;
    ProcTable procs;
    std::string source, line;
    int depth = 0;

    while (true) {
        std::cout << (depth ? ".. " : ">> ") << std::flush;
        if (!std::getline(std::cin, line)) {
            if (depth > 0) {
                try {
                    std::istringstream input(source);
                    read(input, ops);
                } catch (const PocoError& e) {
                    show_error(e);
                }
            }
            break;
        }
        if (depth == 0 && (line == "quit" || line == "exit")) break;

        bool quoted = false;
        for (char c : line) {
            if (c == '"') quoted = !quoted;
            else if (!quoted) {
                if (c == '#') break;
                if (c == '{') ++depth;
                if (c == '}') --depth;
            }
        }
        source += line + '\n';
        if (quoted) {
            std::cerr << "error: string not closed\n";
            source.clear();
            depth = 0;
            continue;
        }
        if (depth > 0) continue;

        try {
            std::istringstream input(source);
            NodePtr program = read(input, ops);
            Block result = exec(as<Block>(program), env, procs);
            for (const auto& n : result) print(n, std::cout) << '\n';
        } catch (const PocoError& e) {
            show_error(e);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
        }
        source.clear();
        depth = 0;
    }
}

#endif // POCO_H
