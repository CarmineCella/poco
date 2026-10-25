// Tiny Vector Scheme, C++17. One numeric type: std::valarray<double>.
// Build: c++ -std=c++17 -O2 -Wall -Wextra -pedantic tiny_vector_scheme.cpp -o tvs
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <valarray>
#include <variant>
#include <vector>
using Array = std::valarray<double>;
constexpr size_t MaxArray = 1000000;                    // allocation guard for this experiment
struct Cell; struct Env;
using Ptr = std::shared_ptr<Cell>;
using Frame = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Prim = Ptr (*)(const List&);
struct Symbol { std::string name; };
struct Lambda { std::vector<std::string> params; Ptr body; Frame env; };
struct Cell : std::variant<Array, Symbol, List, Prim, Lambda> { using variant::variant; };
struct Env { std::unordered_map<std::string, Ptr> vars; Frame parent; };
template<class T> Ptr cell(T x) { return std::make_shared<Cell>(std::move(x)); }
template<class T> bool is(const Ptr& p) { return p && std::holds_alternative<T>(*p); }
template<class T> T& as(const Ptr& p) {
    if (!is<T>(p)) throw std::runtime_error("type error");
    return std::get<T>(*p); }
template<class T> T& as(Ptr&&) = delete;                  // never borrow from a temporary cell
Ptr num(double x) { return cell(Array{x}); }
Ptr nil() { return cell(List{}); }
double scalar(const Ptr& p) {
    const Array& x = as<Array>(p);
    if (x.size() != 1) throw std::runtime_error("scalar expected");
    return x[0]; }
size_t index(const Ptr& p, size_t bound, bool endpoint = false) {
    double x = scalar(p);
    if (!std::isfinite(x) || x < 0 || std::trunc(x) != x ||
        (endpoint ? x > double(bound) : x >= double(bound)))
        throw std::runtime_error("array index out of range");
    return size_t(x); }
Ptr clone(const Ptr& p) {                                // protect source literals from set-ref!
    if (is<Array>(p)) return cell(as<Array>(p));
    if (is<List>(p)) {
        List r; for (const Ptr& x : as<List>(p)) r.push_back(clone(x));
        return cell(std::move(r)); }
    return p; }
void show(const Ptr& p, int depth = 0) {
    if (depth > 300) throw std::runtime_error("print nesting limit");
    if (is<Array>(p)) {
        const Array& a = as<Array>(p);
        if (a.size() == 1) { std::cout << a[0]; return; }
        std::cout << '[';
        for (size_t i = 0; i < a.size(); ++i) std::cout << (i ? " " : "") << a[i];
        std::cout << ']';
    } else if (is<Symbol>(p)) std::cout << as<Symbol>(p).name;
    else if (is<List>(p)) {
        std::cout << '(';
        const List& a = as<List>(p);
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) std::cout << ' ';
            show(a[i], depth + 1); }
        std::cout << ')';
    } else std::cout << "<procedure>"; }
std::string scan(std::istream& in) {
    std::string s;
    for (int c; (c = in.get()) != EOF;) {
        if (c == ';') {
            if (!s.empty()) { in.unget(); return s; }
            while ((c = in.get()) != EOF && c != '\n') {}
            continue; }
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!s.empty()) return s;
            continue; }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '\'') {
            if (!s.empty()) { in.unget(); return s; }
            return std::string(1, char(c)); }
        s += char(c); }
    return s; }
Ptr parse(std::istream& in, const std::string& t, int depth = 0) {
    if (depth > 300) throw std::runtime_error("source nesting limit");
    if (t.empty()) throw std::runtime_error("unexpected EOF");
    if (t == "(") {
        List a;
        for (std::string u = scan(in); u != ")"; u = scan(in)) {
            if (u.empty()) throw std::runtime_error("missing )");
            a.push_back(parse(in, u, depth + 1)); }
        return cell(std::move(a)); }
    if (t == "[") {
        std::vector<double> a;
        for (std::string u = scan(in); u != "]"; u = scan(in)) {
            if (u.empty()) throw std::runtime_error("missing ]");
            if (a.size() == MaxArray) throw std::runtime_error("array too large");
            a.push_back(scalar(parse(in, u, depth + 1))); }
        return cell(a.empty() ? Array(size_t{0}) : Array(a.data(), a.size())); }
    if (t == ")" || t == "]") throw std::runtime_error("unexpected closing delimiter");
    if (t == "'") return cell(List{cell(Symbol{"quote"}), parse(in, scan(in), depth + 1)});
    char* end = nullptr; errno = 0;
    double x = std::strtod(t.c_str(), &end);
    if (end != t.c_str() && *end == '\0') {
        if (errno == ERANGE || !std::isfinite(x)) throw std::runtime_error("invalid number");
        return num(x); }
    return cell(Symbol{t}); }
void arity(const List& a, size_t n) {
    if (a.size() != n) throw std::runtime_error("wrong number of arguments"); }
template<class Op, class Result = Array> Result broadcast(const Array& x, const Array& y) {
    if (!x.size() || !y.size()) throw std::runtime_error("empty array operand");
    if (x.size() == y.size()) return Op{}(x, y);           // native vector/vector valarray op
    if (x.size() == 1) return Op{}(x[0], y);              // scalar/vector
    if (y.size() == 1) return Op{}(x, y[0]);              // vector/scalar
    throw std::runtime_error("incompatible array sizes"); }
template<class Op, int Identity, bool Inverse> Ptr arithmetic(const List& a) {
    if (a.empty()) {
        if (Inverse) throw std::runtime_error("wrong number of arguments");
        return num(Identity); }
    Array r = as<Array>(a[0]);
    if (a.size() == 1 && Inverse) r = broadcast<Op>(Array{double(Identity)}, r);
    for (size_t i = 1; i < a.size(); ++i) r = broadcast<Op>(r, as<Array>(a[i]));
    return cell(std::move(r)); }
template<class Op> Ptr comparison(const List& a) {
    arity(a, 2);
    std::valarray<bool> mask = broadcast<Op, std::valarray<bool>>(as<Array>(a[0]), as<Array>(a[1]));
    Array r(0., mask.size()); r[mask] = 1.;
    return cell(std::move(r)); }
#define UNARY(name, expr) Ptr name(const List& a) { arity(a, 1); const Array& x = as<Array>(a[0]); return cell(Array(expr)); }
UNARY(vsin, std::sin(x)) UNARY(vcos, std::cos(x))
UNARY(vexp, std::exp(x)) UNARY(vlog, std::log(x))
UNARY(vsqrt, std::sqrt(x)) UNARY(vabs, std::abs(x))
UNARY(vneg, -x)
#undef UNARY
struct Pow { template<class X, class Y> auto operator()(const X& x, const Y& y) const { return std::pow(x, y); } };
Ptr vpow(const List& a) { arity(a, 2); return cell(broadcast<Pow>(as<Array>(a[0]), as<Array>(a[1]))); }
Ptr vvec(const List& a) {
    if (a.size() > MaxArray) throw std::runtime_error("array too large");
    std::vector<double> v; v.reserve(a.size());
    for (const Ptr& p : a) v.push_back(scalar(p));
    return cell(v.empty() ? Array(size_t{0}) : Array(v.data(), v.size())); }
Ptr vmake(const List& a) {
    arity(a, 2); size_t n = index(a[0], MaxArray, true);
    return cell(Array(scalar(a[1]), n)); }
Ptr vrange(const List& a) {                              // (range n) => [0 ... n-1]
    arity(a, 1); size_t n = index(a[0], MaxArray, true);
    Array r(n); for (size_t i = 0; i < n; ++i) r[i] = double(i);
    return cell(std::move(r)); }
Ptr vsum(const List& a) { arity(a, 1); const Array& x = as<Array>(a[0]);
    return num(x.size() ? x.sum() : 0.); }
Ptr vsize(const List& a) { arity(a, 1); return num(double(as<Array>(a[0]).size())); }
Ptr vmin(const List& a) { arity(a, 1); const Array& x = as<Array>(a[0]);
    if (!x.size()) throw std::runtime_error("min: empty array");
    return num(x.min()); }
Ptr vmax(const List& a) { arity(a, 1); const Array& x = as<Array>(a[0]);
    if (!x.size()) throw std::runtime_error("max: empty array");
    return num(x.max()); }
Ptr vref(const List& a) { arity(a, 2); const Array& x = as<Array>(a[0]); return num(x[index(a[1], x.size())]); }
Ptr vset(const List& a) { arity(a, 3); Array& x = as<Array>(a[0]);
    size_t i = index(a[1], x.size()); double value = scalar(a[2]); x[i] = value; return a[2]; }
Ptr vlist(const List& a) { return cell(a); }
Ptr vcar(const List& a) { arity(a, 1); const List& l = as<List>(a[0]);
    if (l.empty()) throw std::runtime_error("car: empty list");
    return l[0]; }
Ptr vcdr(const List& a) { arity(a, 1); const List& l = as<List>(a[0]);
    if (l.empty()) throw std::runtime_error("cdr: empty list");
    return cell(List(l.begin() + 1, l.end())); }
Ptr vcons(const List& a) { arity(a, 2); List l{a[0]}; const List& tail = as<List>(a[1]);
    l.insert(l.end(), tail.begin(), tail.end()); return cell(std::move(l)); }
Ptr vnull(const List& a) { arity(a, 1); return num(is<List>(a[0]) && as<List>(a[0]).empty()); }
Ptr vdisplay(const List& a) { for (const Ptr& p : a) { show(p); std::cout << ' '; } std::cout << '\n'; return nil(); }
struct Machine {
    std::vector<std::weak_ptr<Env>> frames; Frame global; int depth = 0;
    Frame frame(Frame parent = {}) {
        auto e = std::make_shared<Env>(); e->parent = std::move(parent); frames.push_back(e);
        if (frames.size() >= 4096) frames.erase(std::remove_if(frames.begin(), frames.end(),
            [](const auto& w) { return w.expired(); }), frames.end());
        return e; }
    Machine() {
        global = frame();
        for (auto [name, fn] : std::vector<std::pair<std::string, Prim>>{
            {"+", arithmetic<std::plus<>, 0, false>}, {"-", arithmetic<std::minus<>, 0, true>},
            {"*", arithmetic<std::multiplies<>, 1, false>}, {"/", arithmetic<std::divides<>, 1, true>},
            {"<", comparison<std::less<>>}, {"<=", comparison<std::less_equal<>>},
            {">", comparison<std::greater<>>}, {">=", comparison<std::greater_equal<>>},
            {"=", comparison<std::equal_to<>>}, {"pow", vpow},
            {"sin", vsin}, {"cos", vcos}, {"exp", vexp}, {"log", vlog},
            {"sqrt", vsqrt}, {"abs", vabs}, {"neg", vneg},
            {"vec", vvec}, {"make", vmake}, {"range", vrange},
            {"sum", vsum}, {"min", vmin}, {"max", vmax}, {"size", vsize},
            {"ref", vref}, {"set-ref!", vset},
            {"list", vlist}, {"car", vcar}, {"cdr", vcdr}, {"cons", vcons},
            {"null?", vnull}, {"display", vdisplay}
        }) global->vars[name] = cell(fn); }
    // Break cyclic closure/environment ownership when the interpreter is destroyed.
    // Unreachable cycles can still persist during a long-running session (no GC).
    ~Machine() { for (auto& w : frames) if (auto e = w.lock()) { e->vars.clear(); e->parent.reset(); } }
    Ptr eval(Ptr x, Frame env) {
        if (++depth > 1000) { --depth; throw std::runtime_error("evaluation nesting limit"); }
        struct Guard { int& n; ~Guard() { --n; } } guard{depth};
        for (;;) {                                       // tail-position calls reuse this C++ frame
            if (is<Symbol>(x)) {
                const std::string& key = as<Symbol>(x).name;
                for (Frame f = env; f; f = f->parent)
                    if (auto it = f->vars.find(key); it != f->vars.end()) return it->second;
                throw std::runtime_error("unknown variable: " + key); }
            if (is<Array>(x)) return clone(x);           // literals must not become mutable AST storage
            if (!is<List>(x) || as<List>(x).empty()) return x;
            const List& a = as<List>(x);
            std::string name = is<Symbol>(a[0]) ? as<Symbol>(a[0]).name : "";
            if (name == "quote") { if (a.size() != 2) throw std::runtime_error("quote: arity"); return clone(a[1]); }
            if (name == "if") {
                if (a.size() != 3 && a.size() != 4) throw std::runtime_error("if: arity");
                x = scalar(eval(a[1], env)) != 0 ? a[2] : a.size() == 4 ? a[3] : nil();
                continue; }
            if (name == "begin") {
                for (size_t i = 1; i + 1 < a.size(); ++i) eval(a[i], env);
                x = a.size() > 1 ? a.back() : nil();
                continue; }
            if (name == "while") {                       // (while condition expression ...)
                if (a.size() < 3) throw std::runtime_error("while: condition and body required");
                while (scalar(eval(a[1], env)) != 0)
                    for (size_t i = 2; i < a.size(); ++i) eval(a[i], env);
                return nil(); }
            if (name == "define" || name == "set!") {
                if (a.size() != 3) throw std::runtime_error("assignment: arity");
                std::string key = as<Symbol>(a[1]).name;
                Ptr value = eval(a[2], env);
                if (name == "define") return env->vars[key] = value;
                for (Frame f = env; f; f = f->parent)
                    if (auto it = f->vars.find(key); it != f->vars.end()) return it->second = value;
                throw std::runtime_error("unknown variable: " + key); }
            if (name == "lambda") {
                if (a.size() != 3) throw std::runtime_error("lambda: arity");
                std::vector<std::string> params;
                for (const Ptr& p : as<List>(a[1])) {
                    std::string key = as<Symbol>(p).name;
                    if (std::find(params.begin(), params.end(), key) != params.end())
                        throw std::runtime_error("duplicate lambda parameter");
                    params.push_back(key); }
                return cell(Lambda{std::move(params), a[2], env}); }
            Ptr fn = eval(a[0], env);
            List args;
            for (size_t i = 1; i < a.size(); ++i) args.push_back(eval(a[i], env));
            if (is<Prim>(fn)) return as<Prim>(fn)(args);
            if (!is<Lambda>(fn)) throw std::runtime_error("not a procedure");
            const Lambda& f = as<Lambda>(fn);
            if (f.params.size() != args.size()) throw std::runtime_error("lambda: arity");
            Frame local = frame(f.env);
            for (size_t i = 0; i < args.size(); ++i) local->vars[f.params[i]] = args[i];
            x = f.body;
            env = std::move(local); } }
    void run(std::istream& in, bool echo) {
        for (std::string t; ; ) {
            if (echo) std::cout << ">> " << std::flush;
            t = scan(in); if (t.empty()) break;
            Ptr result = eval(parse(in, t), global);
            if (echo) { show(result); std::cout << '\n'; } } }
};
int main(int argc, char** argv) {
    try {
        Machine m;
        if (argc > 1) {
            std::ifstream in(argv[1]);
            if (!in) throw std::runtime_error("cannot open input file");
            m.run(in, false);
        } else m.run(std::cin, true);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1; } }
