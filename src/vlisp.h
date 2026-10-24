// vlisp.h -- a Scheme subset where every number is a valarray<double>
// and a scalar is an array of size 1.  [1 2 3] is an array literal.
#ifndef VLISP_H
#define VLISP_H

#include <cctype>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <valarray>
#include <variant>
#include <vector>

// ------------------------------------------------------------------ values
struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using EnvPtr = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Array = std::valarray<double>;
struct Prim { Ptr (*fn)(const List&); int arity; };    // arity -1 = any number of arguments
struct Symbol { std::string name; };
struct Lambda { List params; Ptr body; EnvPtr env; };
struct Cell : std::variant<Symbol, Array, std::string, List, Prim, Lambda> {
    using variant::variant;
};

std::ostream& print(const Ptr& p, std::ostream& out);

// An error carries the expression that was being evaluated when it happened.
// Primitives throw it without a node; the innermost eval attaches its own.
struct Error : std::runtime_error {
    Ptr node;
    Error(const std::string& msg, Ptr n = nullptr) : std::runtime_error(msg), node(std::move(n)) {}
};
[[noreturn]] void error(const std::string& msg, const Ptr& node = nullptr) {
    throw Error(msg, node);
}
void report(const Error& e, std::ostream& out) {
    out << "error: " << e.what();
    if (e.node) print(e.node, out << "\n    in ");
    out << '\n';
}

template<class T> bool is(const Ptr& p) { return std::holds_alternative<T>(*p); }
template<class T> const T& as(const Ptr& p) {
    if (!is<T>(p)) error("type error", p);
    return std::get<T>(*p);
}
template<class T> Ptr cell(T v) { return std::make_shared<Cell>(std::move(v)); }
Ptr sym(const std::string& s) { return cell(Symbol{s}); }
Ptr num(double d) { return cell(Array{d}); }
Ptr nil() { return cell(List{}); }
double scalar(const Ptr& p) {
    if (!is<Array>(p) || as<Array>(p).size() != 1) error("expected a scalar", p);
    return as<Array>(p)[0];
}
int integer(const Ptr& p) {
    double d = scalar(p);
    if (std::trunc(d) != d || std::abs(d) > 1e9) error("expected an integer", p);
    return int(d);
}
const Array& nonempty(const Ptr& p) {
    if (as<Array>(p).size() == 0) error("empty array");
    return as<Array>(p);
}
const List& nonempty_list(const Ptr& p) {
    if (as<List>(p).empty()) error("empty list");
    return as<List>(p);
}
bool truth(const Ptr& p) {
    if (is<Array>(p)) return as<Array>(p).size() > 0 && as<Array>(p)[0] != 0;
    return !(is<List>(p) && as<List>(p).empty());
}
struct Depth {                                             // guards the C++ stack
    static int level;
    Depth(int limit) {
        if (++level > limit) {
            --level;
            error("recursion too deep");
        }
    }
    ~Depth() { --level; }
};
int Depth::level = 0;

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

std::ostream& print(const Ptr& p, std::ostream& out) {
    if (is<Symbol>(p)) return out << as<Symbol>(p).name;
    if (is<std::string>(p)) return out << '"' << as<std::string>(p) << '"';
    if (is<Array>(p)) {
        const Array& a = as<Array>(p);
        if (a.size() == 1) return out << a[0];
        out << '[';
        for (size_t i = 0; i < a.size(); ++i) out << (i ? " " : "") << a[i];
        return out << ']';
    }
    if (!is<List>(p)) return out << "<procedure>";
    out << '(';
    const List& l = as<List>(p);
    for (size_t i = 0; i < l.size(); ++i) print(l[i], out << (i ? " " : ""));
    return out << ')';
}

// ------------------------------------------------------------------ reader
std::vector<std::string> tokenize(std::istream& in) {
    std::vector<std::string> toks;
    std::string tok;
    for (int c = in.get(); c != EOF; c = in.get()) {
        bool delimiter = std::isspace(c) || std::string("\";()[]'").find(c) != std::string::npos;
        if (delimiter && !tok.empty()) toks.push_back(tok);
        if (delimiter) tok.clear();
        if (c == '"') {
            std::getline(in, tok, '"');
            toks.push_back('"' + tok);
            tok.clear();
        } else if (c == ';') {
            std::getline(in, tok);
            tok.clear();
        } else if (delimiter && !std::isspace(c)) {
            toks.push_back(std::string(1, c));
        } else if (!delimiter) {
            tok += c;
        }
    }
    if (!tok.empty()) toks.push_back(tok);
    return toks;
}

// A number is what istream accepts: sign, digits, point, exponent.
// Unlike strtod this rejects 0x10, inf and nan, which stay symbols.
bool is_number(const std::string& t, double& d) {
    std::istringstream s(t);
    return bool(s >> d) && s.eof();
}

Ptr parse(const std::vector<std::string>& toks, size_t& i) {
    Depth guard(5000);
    if (i >= toks.size()) error("unexpected end of input");
    std::string t = toks[i++];
    if (t == "(") {
        List l;
        while (i < toks.size() && toks[i] != ")") l.push_back(parse(toks, i));
        if (i++ >= toks.size()) error("missing )", cell(l));
        return cell(l);
    }
    if (t == "[") {                                           // array literal: scalars only
        std::vector<double> v;
        while (i < toks.size() && toks[i] != "]") v.push_back(scalar(parse(toks, i)));
        if (i++ >= toks.size()) error("missing ]");
        return cell(Array(v.data(), v.size()));
    }
    if (t == ")" || t == "]") error("unexpected " + t);
    if (t == "'") return cell(List{sym("quote"), parse(toks, i)});
    if (t[0] == '"') return cell(t.substr(1));
    double d;
    return is_number(t, d) ? num(d) : sym(t);
}

// ------------------------------------------------------------------ evaluator
Ptr eval(Ptr x, EnvPtr env) {
    Depth guard(10000);
    try {
        while (true) {                                        // loop = tail calls
            if (is<Symbol>(x)) return env->find(x);
            if (!is<List>(x) || as<List>(x).empty()) return x;
            const List& l = as<List>(x);
            std::string head = is<Symbol>(l[0]) ? as<Symbol>(l[0]).name : "";
            if (head == "quote" || head == "define" || head == "set!" || head == "lambda") {
                if (l.size() != (head == "quote" ? 2 : 3)) error("malformed " + head);
            }
            if (head == "quote") return l[1];
            if (head == "define") return env->vars[as<Symbol>(l[1]).name] = eval(l[2], env);
            if (head == "set!") return env->find(l[1]) = eval(l[2], env);
            if (head == "lambda") {
                for (const Ptr& p : as<List>(l[1])) {
                    if (!is<Symbol>(p)) error("parameter must be a symbol", p);
                }
                return cell(Lambda{as<List>(l[1]), l[2], env});
            }
            if (head == "if") {
                if (l.size() != 3 && l.size() != 4) error("malformed if");
                x = truth(eval(l[1], env)) ? l[2] : l.size() == 4 ? l[3] : nil();
                continue;
            }
            if (head == "begin") {
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
                return p.fn(args);
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
    }
}

// ------------------------------------------------------------------ primitives
// Binary operators: Op is a transparent functor (std::plus<> ...), so Op()(x, y)
// resolves to valarray's own operator overloads.  Broadcast picks among the
// three overloads valarray provides: array-array, scalar-array, array-scalar.
template<class Op, class Result> Result broadcast(const Array& x, const Array& y) {
    if (x.size() == y.size()) return Op()(x, y);
    if (x.size() == 1) return Op()(x[0], y);
    if (y.size() == 1) return Op()(x, y[0]);
    error("incompatible sizes");
}

template<class Op> Ptr arith(const List& a) {
    return cell(broadcast<Op, Array>(as<Array>(a[0]), as<Array>(a[1])));
}

template<class Op> Ptr compare(const List& a) {
    std::valarray<bool> mask = broadcast<Op, std::valarray<bool>>(as<Array>(a[0]), as<Array>(a[1]));
    Array r(0.0, mask.size());
    r[mask] = 1.0;
    return cell(r);
}

// Unary functions: one wrapper per <cmath> overload that valarray provides.
template<Array (*F)(const Array&)> Ptr unary(const List& a) {
    return cell(F(as<Array>(a[0])));
}
Array vneg(const Array& x)   { return -x; }
Array vabs(const Array& x)   { return std::abs(x); }
Array vsqrt(const Array& x)  { return std::sqrt(x); }
Array vexp(const Array& x)   { return std::exp(x); }
Array vlog(const Array& x)   { return std::log(x); }
Array vlog10(const Array& x) { return std::log10(x); }
Array vsin(const Array& x)   { return std::sin(x); }
Array vcos(const Array& x)   { return std::cos(x); }
Array vtan(const Array& x)   { return std::tan(x); }
Array vasin(const Array& x)  { return std::asin(x); }
Array vacos(const Array& x)  { return std::acos(x); }
Array vatan(const Array& x)  { return std::atan(x); }
Array vsinh(const Array& x)  { return std::sinh(x); }
Array vcosh(const Array& x)  { return std::cosh(x); }
Array vtanh(const Array& x)  { return std::tanh(x); }

// pow and atan2 are binary <cmath> functions with the same broadcast rules.
struct Pow   { template<class X, class Y> Array operator()(const X& x, const Y& y) const { return std::pow(x, y); } };
struct Atan2 { template<class X, class Y> Array operator()(const X& x, const Y& y) const { return std::atan2(x, y); } };

// valarray members
Ptr vsum(const List& a) { return num(nonempty(a[0]).sum()); }
Ptr vmin(const List& a) { return num(nonempty(a[0]).min()); }
Ptr vmax(const List& a) { return num(nonempty(a[0]).max()); }
Ptr vsize(const List& a) { return num(as<Array>(a[0]).size()); }
Ptr vshift(const List& a) { return cell(as<Array>(a[0]).shift(integer(a[1]))); }
Ptr vcshift(const List& a) { return cell(as<Array>(a[0]).cshift(integer(a[1]))); }
Ptr vref(const List& a) {
    const Array& x = as<Array>(a[0]);
    int i = integer(a[1]);
    if (i < 0 || size_t(i) >= x.size()) error("index out of range", a[1]);
    return num(x[i]);
}

// lists
Ptr car(const List& a) { return nonempty_list(a[0])[0]; }
Ptr cdr(const List& a) {
    const List& l = nonempty_list(a[0]);
    return cell(List(l.begin() + 1, l.end()));
}
Ptr cons(const List& a) {
    List r{a[0]};
    const List& tail = as<List>(a[1]);
    r.insert(r.end(), tail.begin(), tail.end());
    return cell(r);
}
Ptr list(const List& a) { return cell(a); }
Ptr isnull(const List& a) { return num(is<List>(a[0]) && as<List>(a[0]).empty()); }
Ptr display(const List& a) {
    for (const Ptr& x : a) {
        if (is<std::string>(x)) std::cout << as<std::string>(x) << ' ';   // strings without quotes
        else print(x, std::cout) << ' ';
    }
    std::cout << '\n';
    return nil();
}

EnvPtr globals() {
    auto env = std::make_shared<Env>();
    env->vars = {
        {"+", cell(Prim{arith<std::plus<>>, 2})}, {"-", cell(Prim{arith<std::minus<>>, 2})},
        {"*", cell(Prim{arith<std::multiplies<>>, 2})}, {"/", cell(Prim{arith<std::divides<>>, 2})},
        {"pow", cell(Prim{arith<Pow>, 2})}, {"atan2", cell(Prim{arith<Atan2>, 2})},
        {"<", cell(Prim{compare<std::less<>>, 2})}, {"<=", cell(Prim{compare<std::less_equal<>>, 2})},
        {">", cell(Prim{compare<std::greater<>>, 2})}, {">=", cell(Prim{compare<std::greater_equal<>>, 2})},
        {"=", cell(Prim{compare<std::equal_to<>>, 2})}, {"!=", cell(Prim{compare<std::not_equal_to<>>, 2})},
        {"neg", cell(Prim{unary<vneg>, 1})}, {"abs", cell(Prim{unary<vabs>, 1})}, {"sqrt", cell(Prim{unary<vsqrt>, 1})},
        {"exp", cell(Prim{unary<vexp>, 1})}, {"log", cell(Prim{unary<vlog>, 1})}, {"log10", cell(Prim{unary<vlog10>, 1})},
        {"sin", cell(Prim{unary<vsin>, 1})}, {"cos", cell(Prim{unary<vcos>, 1})}, {"tan", cell(Prim{unary<vtan>, 1})},
        {"asin", cell(Prim{unary<vasin>, 1})}, {"acos", cell(Prim{unary<vacos>, 1})}, {"atan", cell(Prim{unary<vatan>, 1})},
        {"sinh", cell(Prim{unary<vsinh>, 1})}, {"cosh", cell(Prim{unary<vcosh>, 1})}, {"tanh", cell(Prim{unary<vtanh>, 1})},
        {"sum", cell(Prim{vsum, 1})}, {"min", cell(Prim{vmin, 1})}, {"max", cell(Prim{vmax, 1})}, {"size", cell(Prim{vsize, 1})},
        {"shift", cell(Prim{vshift, 2})}, {"cshift", cell(Prim{vcshift, 2})}, {"ref", cell(Prim{vref, 2})},
        {"car", cell(Prim{car, 1})}, {"cdr", cell(Prim{cdr, 1})}, {"cons", cell(Prim{cons, 2})}, {"list", cell(Prim{list, -1})},
        {"null?", cell(Prim{isnull, 1})}, {"display", cell(Prim{display, -1})},
    };
    return env;
}

// ------------------------------------------------------------------ interface
void run(std::istream& in, EnvPtr env, bool echo) {
    std::vector<std::string> toks = tokenize(in);
    for (size_t i = 0; i < toks.size();) {
        Ptr v = eval(parse(toks, i), env);
        if (echo) print(v, std::cout) << '\n';
    }
}

// Parenthesis balance of the text typed so far, ignoring strings and comments.
long balance(const std::string& src) {
    long depth = 0;
    bool in_string = false, in_comment = false;
    for (char c : src) {
        if (in_comment) in_comment = c != '\n';
        else if (in_string) in_string = c != '"';
        else if (c == '"') in_string = true;
        else if (c == ';') in_comment = true;
        else if (c == '(' || c == '[') ++depth;
        else if (c == ')' || c == ']') --depth;
    }
    return in_string ? 1 : depth;                              // an open string keeps reading too
}

void repl(EnvPtr env) {
    std::string src, line;
    while (std::cout << (src.empty() ? ">> " : ".. ") << std::flush, std::getline(std::cin, line)) {
        src += line + '\n';
        if (balance(src) > 0) continue;
        try {
            std::istringstream in(src);
            run(in, env, true);
        } catch (Error& e) {
            report(e, std::cerr);
        }
        src.clear();
    }
    if (!src.empty()) error("unexpected end of input");       // EOF inside an open form
}

#endif // VLISP_H
