// vlisp.h
//

#ifndef VLISP_H
#define VLISP_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <valarray>
#include <variant>
#include <vector>

// AST
struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using EnvPtr = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Array = std::valarray<double>;
struct Prim { std::function<Ptr(const List&, const EnvPtr&)> fn; int arity; std::string name = ""; };   // arity -1 = any number
struct Symbol { std::string name; };
struct Lambda { List params; Ptr body; EnvPtr env; };
struct Cell : std::variant<Symbol, Array, std::string, List, Prim, Lambda> {
    using variant::variant;
};
inline std::ostream& print(const Ptr& p, std::ostream& out);
struct Error : std::runtime_error {
    Ptr node;
    Error(const std::string& msg, Ptr n = nullptr) : std::runtime_error(msg), node(std::move(n)) {}
};
[[noreturn]] inline void error(const std::string& msg, const Ptr& node = nullptr) {
    throw Error(msg, node);
}
inline void report(const Error& e, std::ostream& out) {
    out << "error: " << e.what();
    if (e.node) print(e.node, out << "\n    in ");
    out << '\n';
}
template<class T> bool is(const Ptr& p) { return p && std::holds_alternative<T>(*p); }
template<class T> T& as(const Ptr& p) {
    if (!is<T>(p)) error("type error", p);
    return std::get<T>(*p);
}
template<class T> T& as(Ptr&&) = delete;  // a reference into a temporary would dangle
template<class T> Ptr cell(T v) { return std::make_shared<Cell>(std::move(v)); }
inline Ptr sym(const std::string& s) { return cell(Symbol{s}); }
inline Ptr num(double d) { return cell(Array{d}); }
inline Ptr nil() { return cell(List{}); }
inline double scalar(const Ptr& p) {
    if (!is<Array>(p) || as<Array>(p).size() != 1) error("expected a scalar", p);
    return as<Array>(p)[0];
}
inline long integer(const Ptr& p) {                               // a whole number that fits an int
    double d = scalar(p);
    if (!std::isfinite(d) || std::trunc(d) != d || std::abs(d) > 2147483647.0) error("expected an integer", p);
    return long(d);
}
inline size_t index(const Ptr& p, size_t limit) {                 // an integer in [0, limit)
    long i = integer(p);
    if (i < 0 || size_t(i) >= limit) error("index out of range", p);
    return size_t(i);
}
inline const size_t LIMIT = 1 << 27;                              // elements per array: 1 GB of doubles
inline const Array& nonempty(const Ptr& p) {
    if (as<Array>(p).size() == 0) error("empty array");
    return as<Array>(p);
}
inline const List& nonempty_list(const Ptr& p) {
    if (as<List>(p).empty()) error("empty list");
    return as<List>(p);
}
inline bool truth(const Ptr& p) {                                 // an array is true if non-empty and all non-zero
    if (is<Array>(p)) {
        const Array& a = as<Array>(p);
        return a.size() > 0 && std::all_of(std::begin(a), std::end(a), [](double d) { return d != 0; });
    }
    return !(is<List>(p) && as<List>(p).empty());
}
struct Depth {                                             // guards the C++ stack (~400 bytes per eval frame)
    static inline thread_local int level = 0;
    Depth(int limit) {
        if (++level > limit) {
            --level;
            error("recursion too deep");
        }
    }
    ~Depth() { --level; }
};
struct Env {
    std::map<std::string, Ptr> vars;
    EnvPtr parent;
    Ptr& find(const Ptr& symbol) {
        const std::string& name = as<Symbol>(symbol).name;
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (!parent) error("undefined variable", symbol);
        return parent->find(symbol);
    }
};
inline std::ostream& print(const Ptr& p, std::ostream& out) {
    std::setprecision(12);
    if (is<Symbol>(p)) return out << as<Symbol>(p).name;
    if (is<std::string>(p)) {
        out << '"';
        for (char c : as<std::string>(p)) {
            if (c == '\n') out << "\\n";
            else if (c == '\t') out << "\\t";
            else out << (c == '"' || c == '\\' ? "\\" : "") << c;
        }
        return out << '"';
    }
    if (is<Array>(p)) {
        const Array& a = as<Array>(p);
        if (a.size() == 1) return out << a[0];
        out << '[';
        for (size_t i = 0; i < a.size(); ++i) out << (i ? " " : "") << a[i];
        return out << ']';
    }
    if (is<Prim>(p)) return out << as<Prim>(p).name;
    if (is<Lambda>(p)) {                                   // its source: the closure environment is not printed
        const Lambda& f = as<Lambda>(p);
        print(cell(f.params), out << "(lambda ") << ' ';
        return print(f.body, out) << ')';
    }
    out << '(';
    const List& l = as<List>(p);
    for (size_t i = 0; i < l.size(); ++i) print(l[i], out << (i ? " " : ""));
    return out << ')';
}

// READER
struct Reader { std::istream& in; size_t line = 1, token = 1, start = 1; };   // lines of: the input, the last token, the last datum
inline std::string scan(Reader& r) {                              // next token, "" at end of input
    std::string tok;
    for (int c = r.in.get(); c != EOF; c = r.in.get()) {
        bool delimiter = std::isspace(c) || std::string("\";()[]{}'").find(c) != std::string::npos;
        if (!delimiter) {
            if (tok.empty()) r.token = r.line;
            tok += char(c);
            continue;
        }
        if (!tok.empty()) {                                   // the delimiter is the next token: leave it
            r.in.unget();
            return tok;
        }
        if (c == '\n') ++r.line;
        if (std::isspace(c)) continue;
        if (c == ';') {                                       // comment
            while ((c = r.in.get()) != EOF && c != '\n') {}
            ++r.line;
            continue;
        }
        r.token = r.line;
        if (c != '"') return std::string(1, c);
        for (c = r.in.get(); c != '"'; c = r.in.get()) {       // escapes: \" \\ \n \t
            if (c == EOF) error("unterminated string");
            r.line += c == '\n';
            if (c == '\\') {
                c = r.in.get();
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c != '"' && c != '\\') error("unknown escape");
            }
            tok += char(c);
        }
        return '"' + tok;
    }
    return tok;
}
inline bool is_number(const std::string& t, double& d) {
    std::istringstream s(t);
    return bool(s >> d) && s.eof();
}
inline Ptr parse(Reader& r, const std::string& t) {              // the datum that starts with token t
    Depth guard(500);
    if (t.empty()) error("unexpected end of input");
    if (t == "(" || t == "{") {                               // {e1 e2 ...} is (do e1 e2 ...)
        List l = t == "{" ? List{sym("do")} : List{};
        std::string close = t == "(" ? ")" : "}";
        for (std::string u = scan(r); u != close; u = scan(r)) {
            if (u.empty()) error("missing " + close, cell(l));
            l.push_back(parse(r, u));
        }
        return cell(l);
    }
    if (t == "[") {                                           // array literal: scalars only
        std::vector<double> v;
        for (std::string u = scan(r); u != "]"; u = scan(r)) {
            if (u.empty()) error("missing ]");
            v.push_back(scalar(parse(r, u)));
        }
        return cell(Array(v.data(), v.size()));
    }
    if (t == ")" || t == "]" || t == "}") error("unexpected " + t);
    if (t == "'") return cell(List{sym("quote"), parse(r, scan(r))});
    if (t[0] == '"') return cell(t.substr(1));
    double d;
    return is_number(t, d) ? num(d) : sym(t);
}
inline Ptr read(Reader& r) {                                      // next datum, null at end of input
    std::string t = scan(r);
    r.start = r.token;
    return t.empty() ? nullptr : parse(r, t);
}

// EVALUATOR
inline Ptr sugar(const List& l) {                                 // (define (f x) body) = (define f (lambda (x) body))
    const List& sig = as<List>(l[1]);
    if (sig.empty()) error("malformed define");
    return cell(List{sym("define"), sig[0], cell(List{sym("lambda"), cell(List(sig.begin() + 1, sig.end())), l[2]})});
}
inline Ptr clone(const Ptr& p) {                                  // fresh arrays: set-ref! must never edit the program
    if (is<Array>(p)) return cell(as<Array>(p));
    if (!is<List>(p)) return p;
    List r;
    for (const Ptr& x : as<List>(p)) r.push_back(clone(x));
    return cell(r);
}
inline Ptr vapply(const Ptr& f, const Ptr& args) {                // (apply f lst) = (f 'a1 'a2 ...), still a tail call
    List call{f};
    for (const Ptr& p : as<List>(args)) call.push_back(cell(List{sym("quote"), p}));
    return cell(call);
}
inline Ptr eval(Ptr x, EnvPtr env);
inline Ptr loop(const List& l, EnvPtr env) {                      // (for (i n) body): i = 0 .. n-1, bound in a child frame
    if (l.size() != 3 || !is<List>(l[1]) || as<List>(l[1]).size() != 2) error("malformed for");
    const std::string& var = as<Symbol>(as<List>(l[1])[0]).name;
    size_t n = index(eval(as<List>(l[1])[1], env), LIMIT + 1);
    auto local = std::make_shared<Env>();
    local->parent = env;
    for (size_t i = 0; i < n; ++i) {
        local->vars[var] = num(double(i));
        eval(l[2], local);
    }
    return nil();
}

inline Ptr eval(Ptr x, EnvPtr env) {
    Depth guard(2500);
    try {
        while (true) {                                        // loop = tail calls
            if (is<Symbol>(x)) return env->find(x);
            if (!is<List>(x) || as<List>(x).empty()) return clone(x);
            const List& l = as<List>(x);
            std::string head = is<Symbol>(l[0]) ? as<Symbol>(l[0]).name : "";
            if (head == "quote" && l.size() != 2) error("malformed quote");
            if ((head == "define" || head == "set!" || head == "lambda") && l.size() != 3) error("malformed " + head);
            if (head == "quote") return clone(l[1]);
            if (head == "define" && is<List>(l[1])) {
                x = sugar(l);
                continue;
            }
            if (head == "define") return env->vars[as<Symbol>(l[1]).name] = eval(l[2], env);
            if (head == "set!") return env->find(l[1]) = eval(l[2], env);
            if (head == "lambda") {
                const List& params = as<List>(l[1]);
                for (size_t i = 0; i < params.size(); ++i) {
                    if (!is<Symbol>(params[i])) error("parameter must be a symbol", params[i]);
                    for (size_t j = 0; j < i; ++j) if (as<Symbol>(params[j]).name == as<Symbol>(params[i]).name) error("duplicate parameter", params[i]);
                }
                return cell(Lambda{params, l[2], env});
            }
            if (head == "if") {
                if (l.size() != 3 && l.size() != 4) error("malformed if");
                x = truth(eval(l[1], env)) ? l[2] : l.size() == 4 ? l[3] : nil();
                continue;
            }
            if (head == "apply") {
                if (l.size() != 3) error("malformed apply");
                x = vapply(eval(l[1], env), eval(l[2], env));
                continue;
            }
            if (head == "for") return loop(l, env);
            if (head == "do") {
                if (l.size() == 1) return nil();
                for (size_t i = 1; i + 1 < l.size(); ++i) eval(l[i], env);
                x = l.back();
                continue;
            }
            Ptr f = eval(l[0], env);
            List args;
            for (size_t i = 1; i < l.size(); ++i) args.push_back(eval(l[i], env));
            if (is<Prim>(f)) {
                const Prim& p = as<Prim>(f);
                if (p.arity >= 0 && int(args.size()) != p.arity) error("wrong number of arguments");
                return p.fn(args, env);
            }
            if (!is<Lambda>(f)) error("not a procedure", l[0]);
            const Lambda& fn = as<Lambda>(f);
            if (fn.params.size() != args.size()) error("wrong number of arguments");
            auto local = std::make_shared<Env>();
            local->parent = fn.env;
            for (size_t i = 0; i < args.size(); ++i) local->vars[as<Symbol>(fn.params[i]).name] = args[i];
            x = fn.body;
            env = local;
        }
    } catch (Error& e) {
        if (!e.node) e.node = x;                              // attach the innermost expression
        throw;
    } catch (std::exception& e) {                             // bad_alloc, length_error ...: reported like any other error
        throw Error(e.what(), x);
    }
}

// PRIMITIVES
template<class Op, class Result> Result broadcast(const Array& x, const Array& y) {
    if (x.size() == y.size()) return Op()(x, y);
    if (x.size() == 1) return Op()(x[0], y);
    if (y.size() == 1) return Op()(x, y[0]);
    error("incompatible sizes");
}
template<class Op, int Unit> Ptr arith(const List& a, const EnvPtr&) {   // (+ a b c ...); (- x) is (- Unit x): 0 for + -, 1 for * /
    if (a.empty()) error("wrong number of arguments");
    if (a.size() == 1) return cell(broadcast<Op, Array>(Array{double(Unit)}, as<Array>(a[0])));
    Array r = broadcast<Op, Array>(as<Array>(a[0]), as<Array>(a[1]));
    for (size_t i = 2; i < a.size(); ++i) r = broadcast<Op, Array>(r, as<Array>(a[i]));
    return cell(std::move(r));
}
template<class Op> Ptr compare(const List& a, const EnvPtr&) {
    std::valarray<bool> mask = broadcast<Op, std::valarray<bool>>(as<Array>(a[0]), as<Array>(a[1]));
    Array r(0.0, mask.size());
    r[mask] = 1.0;
    return cell(r);
}
inline Ptr unary(Array (*f)(const Array&)) {
    return cell(Prim{[f](const List& a, const EnvPtr&) { return cell(f(as<Array>(a[0]))); }, 1});
}
struct Pow { template<class X, class Y> Array operator()(const X& x, const Y& y) const { return std::pow(x, y); } };
inline Ptr make(const List& a, const EnvPtr&) { return cell(Array(scalar(a[1]), index(a[0], LIMIT + 1))); }
inline Ptr range(const List& a, const EnvPtr&) {                  // (range [start] end [step]), end excluded
    if (a.empty() || a.size() > 3) error("wrong number of arguments");
    double start = a.size() > 1 ? scalar(a[0]) : 0, end = scalar(a[a.size() > 1]), step = a.size() > 2 ? scalar(a[2]) : 1;
    double n = std::ceil((end - start) / step);
    if (!std::isfinite(step) || !(n <= double(LIMIT))) error("invalid range");   // n is nan or inf for a zero step or infinite bounds
    Array r(size_t(std::max(n, 0.0)));
    for (size_t k = 0; k < r.size(); ++k) r[k] = std::fma(double(k), step, start);
    return cell(r);
}
inline Ptr vslice(const List& a, const EnvPtr&) {                 // (slice x start end [step]), end excluded
    if (a.size() != 3 && a.size() != 4) error("wrong number of arguments");
    const Array& x = as<Array>(a[0]);
    size_t start = index(a[1], x.size() + 1), end = index(a[2], x.size() + 1), step = a.size() == 4 ? index(a[3], x.size() + 1) : 1;
    if (end < start || step == 0) error("invalid slice");
    return cell(Array(x[std::slice(start, (end - start + step - 1) / step, step)]));
}
inline Ptr cat(const List& a, const EnvPtr&) {
    const Array& x = as<Array>(a[0]);
    const Array& y = as<Array>(a[1]);
    Array r(x.size() + y.size());
    r[std::slice(0, x.size(), 1)] = x;
    r[std::slice(x.size(), y.size(), 1)] = y;
    return cell(r);
}
inline Ptr array2list(const List& a, const EnvPtr&) {
    List r;
    for (double d : as<Array>(a[0])) r.push_back(num(d));
    return cell(r);
}
inline Ptr list2array(const List& a, const EnvPtr&) {             // the list must hold scalars
    std::vector<double> v;
    for (const Ptr& p : as<List>(a[0])) v.push_back(scalar(p));
    return cell(Array(v.data(), v.size()));
}
inline Ptr vsum(const List& a, const EnvPtr&) { return num(as<Array>(a[0]).size() ? as<Array>(a[0]).sum() : 0); }
inline Ptr vmin(const List& a, const EnvPtr&) { return num(nonempty(a[0]).min()); }
inline Ptr vmax(const List& a, const EnvPtr&) { return num(nonempty(a[0]).max()); }
inline Ptr vcshift(const List& a, const EnvPtr&) { return cell(as<Array>(a[0]).cshift(int(integer(a[1])))); }
inline Ptr vref(const List& a, const EnvPtr&) { return num(as<Array>(a[0])[index(a[1], as<Array>(a[0]).size())]); }
inline Ptr vset(const List& a, const EnvPtr&) {                   // (set-ref! x i v): in place, x is shared
    as<Array>(a[0])[index(a[1], as<Array>(a[0]).size())] = scalar(a[2]);
    return a[2];
}
inline Ptr save(const List& a, const EnvPtr&) {                   // (save "file" x ...): printed form, one per line
    if (a.empty()) error("wrong number of arguments");
    std::ofstream f(as<std::string>(a[0]));
    if (!f) error("cannot open", a[0]);
    f << std::setprecision(17);                             // exact round trip
    for (size_t i = 1; i < a.size(); ++i) print(a[i], f) << '\n';
    if (!f) error("cannot write", a[0]);
    return nil();
}
inline Ptr readfile(const List& a, const EnvPtr&) {               // (read "file"): the list of data in the file, unevaluated
    std::ifstream f(as<std::string>(a[0]));
    if (!f) error("cannot open", a[0]);
    Reader r{f};
    List l;
    for (Ptr x; (x = read(r));) l.push_back(x);
    if (f.bad()) error("cannot read", a[0]);
    return cell(l);
}
inline Ptr load(const List& a, const EnvPtr& env) {               // (load "file"): local folder, then ~/.vlisp/; errors say file:line
    std::string name = as<std::string>(a[0]);
    if (!std::ifstream(name) && std::getenv("HOME")) name = std::string(std::getenv("HOME")) + "/.vlisp/" + name;
    std::ifstream f(name);
    if (!f) error("cannot open", a[0]);
    Reader r{f};
    Ptr v = nil();
    try {
        for (Ptr x; (x = read(r));) v = eval(x, env);
        if (f.bad()) error("cannot read", a[0]);
    } catch (Error& e) {
        error(name + ":" + std::to_string(r.start) + ": " + e.what(), e.node);
    } catch (std::exception& e) {
        error(name + ":" + std::to_string(r.start) + ": " + e.what());
    }
    return v;                                              // the last value: (define f (load "f.vl"))
}

// lists and generic
inline Ptr car(const List& a, const EnvPtr&) { return nonempty_list(a[0])[0]; }
inline Ptr cdr(const List& a, const EnvPtr&) {
    const List& l = nonempty_list(a[0]);
    return cell(List(l.begin() + 1, l.end()));
}
inline Ptr cons(const List& a, const EnvPtr&) {
    List r{a[0]};
    const List& tail = as<List>(a[1]);
    r.insert(r.end(), tail.begin(), tail.end());
    return cell(r);
}
inline Ptr list(const List& a, const EnvPtr&) { return cell(a); }
inline bool vequal(const Ptr& a, const Ptr& b) {                  // structural; procedures by identity
    if (a->index() != b->index()) return false;
    if (is<Symbol>(a)) return as<Symbol>(a).name == as<Symbol>(b).name;
    if (is<std::string>(a)) return as<std::string>(a) == as<std::string>(b);
    if (is<Array>(a)) {
        const Array& x = as<Array>(a);
        const Array& y = as<Array>(b);
        return x.size() == y.size() && (x.size() == 0 || (x == y).min());
    }
    if (is<List>(a)) {
        const List& x = as<List>(a);
        const List& y = as<List>(b);
        return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin(), vequal);
    }
    return a == b;
}
inline Ptr isequal(const List& a, const EnvPtr&) { return num(vequal(a[0], a[1])); }
inline Ptr isnull(const List& a, const EnvPtr&) { return num(is<List>(a[0]) && as<List>(a[0]).empty()); }
inline Ptr len(const List& a, const EnvPtr&) {                    // of an array, a list or a string
    if (is<List>(a[0])) return num(as<List>(a[0]).size());
    if (is<std::string>(a[0])) return num(as<std::string>(a[0]).size());
    return num(as<Array>(a[0]).size());
}
inline Ptr type(const List& a, const EnvPtr&) {
    static const char* names[] = {"symbol", "array", "string", "list", "primitive", "procedure"};   // variant order
    return cell(std::string(names[a[0]->index()]));
}
inline Ptr names(const List&, const EnvPtr& env) {                // (env): the symbols bound here and in enclosing frames
    List r;
    for (EnvPtr e = env; e; e = e->parent) for (const auto& [name, p] : e->vars) r.push_back(sym(name));
    return cell(r);
}
inline Ptr display(const List& a, const EnvPtr&) {
    for (const Ptr& x : a) {
        if (is<std::string>(x)) std::cout << as<std::string>(x) << ' ';   // strings without quotes
        else print(x, std::cout) << ' ';
    }
    std::cout << '\n';
    return nil();
}

// INTERFACE
inline EnvPtr globals() {
    auto env = std::make_shared<Env>();
    env->vars = {
        {"+", cell(Prim{arith<std::plus<>, 0>, -1})}, {"-", cell(Prim{arith<std::minus<>, 0>, -1})},
        {"*", cell(Prim{arith<std::multiplies<>, 1>, -1})}, {"/", cell(Prim{arith<std::divides<>, 1>, -1})},
        {"pow", cell(Prim{arith<Pow, 1>, 2})},
        {"<", cell(Prim{compare<std::less<>>, 2})}, {"<=", cell(Prim{compare<std::less_equal<>>, 2})},
        {">", cell(Prim{compare<std::greater<>>, 2})}, {">=", cell(Prim{compare<std::greater_equal<>>, 2})},
        {"=", cell(Prim{compare<std::equal_to<>>, 2})}, {"!=", cell(Prim{compare<std::not_equal_to<>>, 2})},
        {"abs", unary([](const Array& x) { return Array(std::abs(x)); })},
        {"sqrt", unary([](const Array& x) { return Array(std::sqrt(x)); })},
        {"exp", unary([](const Array& x) { return Array(std::exp(x)); })},
        {"log", unary([](const Array& x) { return Array(std::log(x)); })},
        {"sin", unary([](const Array& x) { return Array(std::sin(x)); })},
        {"cos", unary([](const Array& x) { return Array(std::cos(x)); })},
        {"tan", unary([](const Array& x) { return Array(std::tan(x)); })},
        {"atan", unary([](const Array& x) { return Array(std::atan(x)); })},
        {"floor", unary([](const Array& x) { return Array(x.apply([](double d) { return std::floor(d); })); })},
        {"make", cell(Prim{make, 2})}, {"range", cell(Prim{range, -1})}, {"slice", cell(Prim{vslice, -1})},
        {"cat", cell(Prim{cat, 2})}, {"cshift", cell(Prim{vcshift, 2})},
        {"sum", cell(Prim{vsum, 1})}, {"min", cell(Prim{vmin, 1})}, {"max", cell(Prim{vmax, 1})},
        {"ref", cell(Prim{vref, 2})}, {"set-ref!", cell(Prim{vset, 3})},
        {"array->list", cell(Prim{array2list, 1})}, {"list->array", cell(Prim{list2array, 1})},
        {"save", cell(Prim{save, -1})}, {"read", cell(Prim{readfile, 1})}, {"load", cell(Prim{load, 1})},
        {"car", cell(Prim{car, 1})}, {"cdr", cell(Prim{cdr, 1})}, {"cons", cell(Prim{cons, 2})}, {"list", cell(Prim{list, -1})},
        {"null?", cell(Prim{isnull, 1})}, {"equal?", cell(Prim{isequal, 2})}, {"len", cell(Prim{len, 1})},
        {"type", cell(Prim{type, 1})}, {"env", cell(Prim{names, 0})}, {"display", cell(Prim{display, -1})},
    };
    for (auto& [name, p] : env->vars) as<Prim>(p).name = name;   // so that primitives print as themselves
    return env;
}
inline void repl(EnvPtr env) {
    Reader r{std::cin};
    while (std::cout << ">> " << std::flush) {
        try {
            Ptr x = read(r);
            if (!x) break;
            print(eval(x, env), std::cout) << '\n';
            continue;
        } catch (Error& e) {
            report(e, std::cerr);
        } catch (std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
        }
        r.in.ignore(1 << 30, '\n');                           // after an error the rest of the line is dropped
        ++r.line;
    }
}

#endif // VLISP_H

// eof
