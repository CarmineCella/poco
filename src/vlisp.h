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
#include <string_view>

// AST
struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using EnvPtr = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Array = std::valarray<double>;
struct Prim { Ptr (*fn)(const List&, const EnvPtr&); std::string sig, name; int arity; };   // arity -1: the primitive checks
struct Symbol { std::string name; };
struct Lambda { List params; Ptr body; EnvPtr env; };
struct Cell : std::variant<Symbol, Array, std::string, List, Prim, Lambda> {
    using variant::variant;
};
inline std::ostream& print(const Ptr& p, std::ostream& out);
struct Error : std::runtime_error { Ptr node; Error(const std::string& m, Ptr n = nullptr) : runtime_error(m), node(n) {} };
[[noreturn]] inline void error(const std::string& msg, const Ptr& node = nullptr) { throw Error(msg, node); }
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
inline size_t index(const Ptr& p, size_t limit) {                 // an integer in [0, limit); NaN fails every test
    double d = scalar(p);
    if (!(d >= 0 && d < double(limit) && std::trunc(d) == d)) error("index out of range", p);
    return size_t(d);
}
inline const size_t LIMIT = 1 << 27;                              // elements per array: 1 GB of doubles
inline size_t length(const Ptr& p) {                             // of an array, a list or a string
    return is<List>(p) ? as<List>(p).size() : is<std::string>(p) ? as<std::string>(p).size() : as<Array>(p).size();
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
inline Ptr read(Reader& r) { std::string t = scan(r); r.start = r.token; return t.empty() ? nullptr : parse(r, t); }   // null at end

// EVALUATOR
inline Ptr sugar(const List& l) {                                 // (define (f x) body) = (define f (lambda (x) body))
    const List& sig = as<List>(l[1]);
    if (sig.empty()) error("malformed define");
    return cell(List{sym("define"), sig[0], cell(List{sym("lambda"), cell(List(sig.begin() + 1, sig.end())), l[2]})});
}
inline Ptr vapply(const Ptr& f, const Ptr& args) {                // (apply f lst) = (f 'a1 'a2 ...), still a tail call
    List call{f};
    for (const Ptr& p : as<List>(args)) call.push_back(cell(List{sym("quote"), p}));
    return cell(call);
}
inline Ptr eval(Ptr x, EnvPtr env) {
    Depth guard(2500);
    try {
        while (true) {                                        // loop = tail calls
            if (is<Symbol>(x)) return env->find(x);
            if (!is<List>(x) || as<List>(x).empty()) return x;           // values are immutable, so literals are shared
            const List& l = as<List>(x);
            std::string_view head = is<Symbol>(l[0]) ? std::string_view(as<Symbol>(l[0]).name) : std::string_view();
            if (head == "quote" && l.size() != 2) error("malformed quote");
            if ((head == "define" || head == "set!" || head == "lambda") && l.size() != 3) error("malformed " + std::string(head));
            if (head == "quote") return l[1];
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
            if (head == "do") {
                if (l.size() == 1) return nil();
                for (size_t i = 1; i + 1 < l.size(); ++i) eval(l[i], env);
                x = l.back();
                continue;
            }
            Ptr f = eval(l[0], env);
            List args;
            args.reserve(l.size() - 1);
            for (size_t i = 1; i < l.size(); ++i) args.push_back(eval(l[i], env));            
            if (is<Prim>(f)) {
                if (as<Prim>(f).arity >= 0 && int(args.size()) != as<Prim>(f).arity) error("wrong number of arguments");
                return as<Prim>(f).fn(args, env);
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
template<double (*F)(double)> Ptr unary(const List& a, const EnvPtr&) { return cell(Array(as<Array>(a[0]).apply(F))); }
struct Pow { template<class X, class Y> Array operator()(const X& x, const Y& y) const { return std::pow(x, y); } };
inline Ptr build(const List& a, const EnvPtr&) {                  // (build n f): (f 0) .. (f n-1); an array if all are scalars, else a list
    List r(index(a[0], LIMIT + 1));
    for (size_t i = 0; i < r.size(); ++i) r[i] = eval(cell(List{a[1], num(double(i))}), std::make_shared<Env>());
    if (!std::all_of(r.begin(), r.end(), [](const Ptr& p) { return is<Array>(p) && as<Array>(p).size() == 1; })) return cell(r);
    Array x(r.size());
    for (size_t i = 0; i < r.size(); ++i) x[i] = as<Array>(r[i])[0];
    return cell(x);
}
inline Ptr range(const List& a, const EnvPtr&) {                  // (range [start] end [step]), end excluded
    if (a.empty() || a.size() > 3) error("wrong number of arguments");
    double start = a.size() > 1 ? scalar(a[0]) : 0, end = scalar(a[a.size() > 1]), step = a.size() > 2 ? scalar(a[2]) : 1;
    double n = std::ceil((end - start) / step);
    if (!std::isfinite(step) || !(n <= double(LIMIT))) error("invalid range");   // n is nan or inf for a zero step or infinite bounds
    Array r(size_t(std::max(n, 0.0)));
    for (size_t k = 0; k < r.size(); ++k) r[k] = std::fma(double(k), step, start);
    return cell(r);
}
template<class C> Ptr take(const C& c, size_t s, size_t e, size_t step) {   // elements s, s+step, ... below e
    C r;
    for (size_t i = s; i < e; i += step) r.push_back(c[i]);
    return cell(r);
}
inline Ptr vslice(const List& a, const EnvPtr&) {                 // (slice x start end [step]), end excluded: arrays, lists, strings
    if (a.size() != 3 && a.size() != 4) error("wrong number of arguments");
    size_t n = length(a[0]), s = index(a[1], n + 1), e = index(a[2], n + 1), step = a.size() == 4 ? index(a[3], n + 1) : 1;
    if (e < s || step == 0) error("invalid slice");
    if (is<List>(a[0])) return take(as<List>(a[0]), s, e, step);
    if (is<std::string>(a[0])) return take(as<std::string>(a[0]), s, e, step);
    return cell(Array(as<Array>(a[0])[std::slice(s, (e - s + step - 1) / step, step)]));
}
inline Ptr cat(const List& a, const EnvPtr&) {                    // (cat x y): arrays, lists or strings
    if (is<std::string>(a[0])) return cell(as<std::string>(a[0]) + as<std::string>(a[1]));
    if (is<List>(a[0])) {
        List r = as<List>(a[0]);
        r.insert(r.end(), as<List>(a[1]).begin(), as<List>(a[1]).end());
        return cell(r);
    }
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
inline Ptr vmax(const List& a, const EnvPtr&) { if (!as<Array>(a[0]).size()) error("empty array", a[0]); return num(as<Array>(a[0]).max()); }
inline Ptr vref(const List& a, const EnvPtr&) {                   // (ref x i): an element of an array, a list or a string
    size_t i = index(a[1], length(a[0]));
    if (is<List>(a[0])) return as<List>(a[0])[i];
    if (is<std::string>(a[0])) return cell(std::string(1, as<std::string>(a[0])[i]));
    return num(as<Array>(a[0])[i]);
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

// lists, strings and generic
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
inline Ptr len(const List& a, const EnvPtr&) { return num(length(a[0])); }
inline Ptr type(const List& a, const EnvPtr&) {
    static const char* names[] = {"symbol", "array", "string", "list", "primitive", "procedure"};   // variant order
    return cell(std::string(names[a[0]->index()]));
}
inline Ptr help(const List& a, const EnvPtr& env) {               // (help): what can be called, as call templates; (help 'f): f's source
    if (a.size() > 1) error("wrong number of arguments");
    if (a.size() == 1) {
        Ptr p = env->find(a[0]);
        if (is<Prim>(p)) std::cout << '(' << as<Prim>(p).name << ' ' << as<Prim>(p).sig << ")\n"; else print(p, std::cout) << '\n';
        return nil();
    }
    std::cout << "forms: (quote x) (if test then else) (define name value) (define (name args) body) (set! name value)"
                 " (lambda (args) body) (do e ...) {e ...} (apply f list)\n";
    for (int pass = 0; pass < 2; ++pass) {                           // primitives, then everything else
        std::cout << (pass ? "defined:" : "primitives:");
        for (EnvPtr e = env; e; e = e->parent) for (const auto& [name, p] : e->vars) {
            if (is<Prim>(p) == bool(pass)) continue;
            if (is<Prim>(p)) std::cout << " (" << name << (as<Prim>(p).sig.empty() ? "" : " ") << as<Prim>(p).sig << ')';
            else if (is<Lambda>(p)) { std::cout << " (" << name; for (const Ptr& q : as<Lambda>(p).params) print(q, std::cout << ' '); std::cout << ')'; }
            else if (is<Array>(p) && as<Array>(p).size() != 1) std::cout << ' ' << name << " = array[" << as<Array>(p).size() << ']';
            else if (is<List>(p)) std::cout << ' ' << name << " = list[" << as<List>(p).size() << ']';
            else print(p, std::cout << ' ' << name << " = ");
        }
        std::cout << '\n';
    }
    return nil();
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
    auto def = [&](const char* name, Ptr (*fn)(const List&, const EnvPtr&), std::string sig) {   // "[x]" or "..." in sig: variadic
        int arity = sig.find_first_of("[.") != std::string::npos ? -1 : sig.empty() ? 0 : int(std::count(sig.begin(), sig.end(), ' ')) + 1;
        env->vars[name] = cell(Prim{fn, sig, name, arity});
    };
    def("+", arith<std::plus<>, 0>, "x y ...");  def("-", arith<std::minus<>, 0>, "x y ...");  def("pow", arith<Pow, 1>, "x y");
    def("*", arith<std::multiplies<>, 1>, "x y ...");  def("/", arith<std::divides<>, 1>, "x y ...");
    def("<", compare<std::less<>>, "x y");  def("<=", compare<std::less_equal<>>, "x y");  def("=", compare<std::equal_to<>>, "x y");
    def(">", compare<std::greater<>>, "x y");  def(">=", compare<std::greater_equal<>>, "x y");  def("!=", compare<std::not_equal_to<>>, "x y");
    def("abs", unary<std::fabs>, "x");  def("sqrt", unary<std::sqrt>, "x");  def("exp", unary<std::exp>, "x");  def("log", unary<std::log>, "x");
    def("sin", unary<std::sin>, "x");  def("cos", unary<std::cos>, "x");  def("tan", unary<std::tan>, "x");  def("atan", unary<std::atan>, "x");
    def("floor", unary<std::floor>, "x");  def("range", range, "[start] end [step]");  def("slice", vslice, "x start end [step]");
    def("build", build, "n f");  def("cat", cat, "x y");  def("sum", vsum, "x");  def("max", vmax, "x");  def("ref", vref, "x i");
    def("array->list", array2list, "x");  def("list->array", list2array, "l");
    def("save", save, "file x ...");  def("read", readfile, "file");  def("load", load, "file");
    def("list", list, "x ...");
    def("equal?", isequal, "x y");  def("len", len, "x");  def("type", type, "x");  def("help", help, "[name]");  def("display", display, "x ...");
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
