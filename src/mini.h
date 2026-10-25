// mini.h -- the smallest vlisp: a Scheme subset where every number is an immutable valarray<double>
// and a scalar is an array of size 1.  [1 2 3] is an array literal, {e1 e2} is (do e1 e2).
#ifndef MINI_H
#define MINI_H

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <valarray>
#include <variant>
#include <vector>

// values
struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using EnvPtr = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Array = std::valarray<double>;
struct Prim { Ptr (*fn)(const List&); int arity; };        // arity -1 = any number of arguments
struct Symbol { std::string name; };
struct Lambda { List params; Ptr body; EnvPtr env; };
struct Cell : std::variant<Symbol, Array, List, Prim, Lambda> { using variant::variant; };
struct Error : std::runtime_error { Ptr node; Error(const std::string& m, Ptr n = nullptr) : runtime_error(m), node(n) {} };
[[noreturn]] inline void error(const std::string& msg, const Ptr& node = nullptr) { throw Error(msg, node); }
template<class T> bool is(const Ptr& p) { return p && std::holds_alternative<T>(*p); }
template<class T> T& as(const Ptr& p) {
    if (!is<T>(p)) error("type error", p);
    return std::get<T>(*p);
}
template<class T> T& as(Ptr&&) = delete;                   // a reference into a temporary would dangle
template<class T> Ptr cell(T v) { return std::make_shared<Cell>(std::move(v)); }
inline Ptr sym(const std::string& s) { return cell(Symbol{s}); }
inline Ptr num(double d) { return cell(Array{d}); }
inline Ptr nil() { return cell(List{}); }
inline double scalar(const Ptr& p) {
    if (!is<Array>(p) || as<Array>(p).size() != 1) error("expected a scalar", p);
    return as<Array>(p)[0];
}
inline size_t index(const Ptr& p, size_t limit) {          // an integer in [0, limit); NaN fails every test
    double d = scalar(p);
    if (!(d >= 0 && d < double(limit) && std::trunc(d) == d)) error("index out of range", p);
    return size_t(d);
}
inline bool truth(const Ptr& p) {                          // an array is true if non-empty and all non-zero
    if (is<Array>(p)) return as<Array>(p).size() && (as<Array>(p) != 0.0).min();
    return !(is<List>(p) && as<List>(p).empty());
}
struct Depth {                                             // guards the C++ stack
    static inline thread_local int level = 0;
    Depth(int limit) { if (++level > limit) { --level; error("recursion too deep"); } }
    ~Depth() { --level; }
};
struct Env {
    std::map<std::string, Ptr> vars;
    EnvPtr parent;
    Ptr& find(const Ptr& s) {
        for (Env* e = this; e; e = e->parent.get()) {
            auto it = e->vars.find(as<Symbol>(s).name);
            if (it != e->vars.end()) return it->second;
        }
        error("undefined variable", s);
    }
};
inline std::ostream& print(const Ptr& p, std::ostream& out) {
    if (is<Symbol>(p)) return out << as<Symbol>(p).name;
    if (is<Array>(p) && as<Array>(p).size() == 1) return out << as<Array>(p)[0];
    if (!is<Array>(p) && !is<List>(p)) return out << "<procedure>";
    out << (is<Array>(p) ? '[' : '(');
    if (is<Array>(p)) for (size_t i = 0; i < as<Array>(p).size(); ++i) out << (i ? " " : "") << as<Array>(p)[i];
    else for (size_t i = 0; i < as<List>(p).size(); ++i) print(as<List>(p)[i], out << (i ? " " : ""));
    return out << (is<Array>(p) ? ']' : ')');
}

// reader
inline std::string scan(std::istream& in) {                // next token, "" at end of input
    std::string tok;
    for (int c = in.get(); c != EOF; c = in.get()) {
        bool delimiter = std::isspace(c) || std::string(";()[]{}'").find(c) != std::string::npos;
        if (!delimiter) { tok += char(c); continue; }
        if (!tok.empty()) { in.unget(); return tok; }        // the delimiter is the next token
        if (c == ';') while ((c = in.get()) != EOF && c != '\n') {}
        else if (!std::isspace(c)) return std::string(1, c);
    }
    return tok;
}
inline Ptr parse(std::istream& in, const std::string& t) {   // the datum that starts with token t
    Depth guard(500);
    if (t.empty()) error("unexpected end of input");
    if (t == "(" || t == "{") {                               // {e1 e2 ...} is (do e1 e2 ...)
        List l = t == "{" ? List{sym("do")} : List{};
        for (std::string u = scan(in); u != (t == "(" ? ")" : "}"); u = scan(in)) l.push_back(parse(in, u));
        return cell(l);
    }
    if (t == "[") {                                           // array literal: scalars only
        std::vector<double> v;
        for (std::string u = scan(in); u != "]"; u = scan(in)) v.push_back(scalar(parse(in, u)));
        return cell(Array(v.data(), v.size()));
    }
    if (t == ")" || t == "]" || t == "}") error("unexpected " + t);
    if (t == "'") return cell(List{sym("quote"), parse(in, scan(in))});
    char* end;
    double d = std::strtod(t.c_str(), &end);
    return *end ? sym(t) : num(d);
}
inline Ptr read(std::istream& in) { std::string t = scan(in); return t.empty() ? nullptr : parse(in, t); }   // null at end of input

// evaluator
inline Ptr eval(Ptr x, EnvPtr env) {
    Depth guard(2500);
    try {
        while (true) {                                        // loop = tail calls
            if (is<Symbol>(x)) return env->find(x);
            if (!is<List>(x) || as<List>(x).empty()) return x;           // arrays are immutable: literals can be shared
            const List& l = as<List>(x);
            std::string head = is<Symbol>(l[0]) ? as<Symbol>(l[0]).name : "";
            if ((head == "quote" && l.size() != 2) || ((head == "define" || head == "set!" || head == "lambda") && l.size() != 3)) error("malformed " + head);
            if (head == "quote") return l[1];
            if (head == "define") return env->vars[as<Symbol>(l[1]).name] = eval(l[2], env);
            if (head == "set!") return env->find(l[1]) = eval(l[2], env);
            if (head == "lambda") {
                for (const Ptr& p : as<List>(l[1])) if (!is<Symbol>(p)) error("parameter must be a symbol", p);
                return cell(Lambda{as<List>(l[1]), l[2], env});
            }
            if (head == "if") {
                if (l.size() != 3 && l.size() != 4) error("malformed if");
                x = truth(eval(l[1], env)) ? l[2] : l.size() == 4 ? l[3] : nil();
                continue;
            }
            if (head == "do") {
                for (size_t i = 1; i + 1 < l.size(); ++i) eval(l[i], env);
                if (l.size() == 1) return nil();
                x = l.back();
                continue;
            }
            Ptr f = eval(l[0], env);
            List args;
            for (size_t i = 1; i < l.size(); ++i) args.push_back(eval(l[i], env));
            if (is<Prim>(f) && as<Prim>(f).arity >= 0 && int(args.size()) != as<Prim>(f).arity) error("wrong number of arguments");
            if (is<Prim>(f)) return as<Prim>(f).fn(args);
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
    } catch (std::exception& e) {                             // bad_alloc and friends, reported like any other error
        throw Error(e.what(), x);
    }
}

// primitives
template<class Op, class R> R broadcast(const Array& x, const Array& y) {   // valarray's three overloads
    if (x.size() == y.size()) return Op()(x, y);
    if (x.size() == 1) return Op()(x[0], y);
    if (y.size() == 1) return Op()(x, y[0]);
    error("incompatible sizes");
}
template<class Op> Ptr arith(const List& a) { return cell(broadcast<Op, Array>(as<Array>(a[0]), as<Array>(a[1]))); }
template<class Op> Ptr compare(const List& a) {            // a mask of 0 and 1
    std::valarray<bool> m = broadcast<Op, std::valarray<bool>>(as<Array>(a[0]), as<Array>(a[1]));
    Array r(0.0, m.size());
    r[m] = 1.0;
    return cell(r);
}
template<double (*F)(double)> Ptr unary(const List& a) { return cell(Array(as<Array>(a[0]).apply(F))); }
inline Ptr range(const List& a) {                          // (range n): 0 .. n-1
    Array r(index(a[0], 1 << 27));
    for (size_t i = 0; i < r.size(); ++i) r[i] = double(i);
    return cell(r);
}
inline Ptr slice(const List& a) {                          // (slice x start n): n elements from start
    size_t s = index(a[1], as<Array>(a[0]).size() + 1), n = index(a[2], as<Array>(a[0]).size() - s + 1);
    return cell(Array(as<Array>(a[0])[std::slice(s, n, 1)]));
}
inline Ptr cat(const List& a) {
    Array r(as<Array>(a[0]).size() + as<Array>(a[1]).size());
    r[std::slice(0, as<Array>(a[0]).size(), 1)] = as<Array>(a[0]);
    r[std::slice(as<Array>(a[0]).size(), as<Array>(a[1]).size(), 1)] = as<Array>(a[1]);
    return cell(r);
}
inline Ptr len(const List& a) { return num(is<List>(a[0]) ? as<List>(a[0]).size() : as<Array>(a[0]).size()); }
inline Ptr sum(const List& a) { return num(as<Array>(a[0]).size() ? as<Array>(a[0]).sum() : 0); }
inline Ptr max(const List& a) { if (!as<Array>(a[0]).size()) error("empty array", a[0]); return num(as<Array>(a[0]).max()); }
inline Ptr ref(const List& a) { return num(as<Array>(a[0])[index(a[1], as<Array>(a[0]).size())]); }
inline Ptr car(const List& a) { if (as<List>(a[0]).empty()) error("empty list"); return as<List>(a[0])[0]; }
inline Ptr cdr(const List& a) { if (as<List>(a[0]).empty()) error("empty list"); return cell(List(as<List>(a[0]).begin() + 1, as<List>(a[0]).end())); }
inline Ptr cons(const List& a) {
    List r{a[0]};
    r.insert(r.end(), as<List>(a[1]).begin(), as<List>(a[1]).end());
    return cell(r);
}
inline Ptr list(const List& a) { return cell(a); }
inline Ptr display(const List& a) {
    for (const Ptr& x : a) print(x, std::cout) << ' ';
    return std::cout << '\n', nil();
}
inline EnvPtr globals() {
    auto env = std::make_shared<Env>();
    auto def = [&](const char* name, Ptr (*fn)(const List&), int arity) { env->vars[name] = cell(Prim{fn, arity}); };
    def("+", arith<std::plus<>>, 2);   def("-", arith<std::minus<>>, 2);   def("*", arith<std::multiplies<>>, 2);
    def("/", arith<std::divides<>>, 2);  def("<", compare<std::less<>>, 2);  def("=", compare<std::equal_to<>>, 2);
    def("sqrt", unary<std::sqrt>, 1);  def("exp", unary<std::exp>, 1);  def("log", unary<std::log>, 1);  def("floor", unary<std::floor>, 1);
    def("sin", unary<std::sin>, 1);  def("cos", unary<std::cos>, 1);  def("range", range, 1);  def("slice", slice, 3);  def("cat", cat, 2);
    def("len", len, 1);  def("sum", sum, 1);  def("max", max, 1);  def("ref", ref, 2);  def("display", display, -1);
    def("car", car, 1);  def("cdr", cdr, 1);  def("cons", cons, 2);  def("list", list, -1);
    return env;
}

#endif // MINI_H
