// lisp.h -- a Scheme subset: quote if define set! lambda begin, closures, lists
#ifndef LISP_H
#define LISP_H

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// ------------------------------------------------------------------ values
struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using EnvPtr = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Prim = Ptr (*)(const List&);                 // primitive on a list of values
using Binary = double (*)(double, double);         // primitive on two numbers
struct Symbol { std::string name; };
struct Lambda { List params; Ptr body; EnvPtr env; };
struct Cell : std::variant<Symbol, double, std::string, List, Prim, Binary, Lambda> {
    using variant::variant;
};

template<class T> bool is(const Ptr& p) { return std::holds_alternative<T>(*p); }
template<class T> const T& as(const Ptr& p) {
    if (!is<T>(p)) throw std::runtime_error("type error");
    return std::get<T>(*p);
}
template<class T> Ptr cell(T v) { return std::make_shared<Cell>(std::move(v)); }
Ptr sym(const std::string& s) { return cell(Symbol{s}); }
Ptr nil() { return cell(List{}); }
bool truth(const Ptr& p) {
    return !(is<double>(p) && as<double>(p) == 0) && !(is<List>(p) && as<List>(p).empty());
}

struct Env {
    std::map<std::string, Ptr> vars;
    EnvPtr parent;
    Ptr& find(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (!parent) throw std::runtime_error("undefined " + name);
        return parent->find(name);
    }
};

std::ostream& print(const Ptr& p, std::ostream& out) {
    if (is<double>(p)) return out << as<double>(p);
    if (is<Symbol>(p)) return out << as<Symbol>(p).name;
    if (is<std::string>(p)) return out << as<std::string>(p);
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
        if (c == '"' || c == ';' || c == '(' || c == ')' || c == '\'' || std::isspace(c)) {
            if (!tok.empty()) toks.push_back(tok);
            tok.clear();
        }
        if (c == '"') {
            std::getline(in, tok, '"');
            toks.push_back('"' + tok);
            tok.clear();
        } else if (c == ';') {
            std::getline(in, tok);
            tok.clear();
        } else if (c == '(' || c == ')' || c == '\'') {
            toks.push_back(std::string(1, c));
        } else if (!std::isspace(c)) {
            tok += c;
        }
    }
    if (!tok.empty()) toks.push_back(tok);
    return toks;
}

Ptr parse(const std::vector<std::string>& toks, size_t& i) {
    if (i >= toks.size()) throw std::runtime_error("unexpected end of input");
    std::string t = toks[i++];
    if (t == "(") {
        List l;
        while (i < toks.size() && toks[i] != ")") l.push_back(parse(toks, i));
        if (i++ >= toks.size()) throw std::runtime_error("missing )");
        return cell(l);
    }
    if (t == ")") throw std::runtime_error("unexpected )");
    if (t == "'") return cell(List{sym("quote"), parse(toks, i)});
    if (t[0] == '"') return cell(t.substr(1));
    char* end;
    double d = std::strtod(t.c_str(), &end);
    return *end == 0 ? cell(d) : sym(t);
}

// ------------------------------------------------------------------ evaluator
Ptr eval(Ptr x, EnvPtr env) {
    while (true) {                                            // loop = tail calls
        if (is<Symbol>(x)) return env->find(as<Symbol>(x).name);
        if (!is<List>(x) || as<List>(x).empty()) return x;
        const List& l = as<List>(x);
        std::string head = is<Symbol>(l[0]) ? as<Symbol>(l[0]).name : "";
        if (head == "quote") return l.at(1);
        if (head == "define") return env->vars[as<Symbol>(l.at(1)).name] = eval(l.at(2), env);
        if (head == "set!") return env->find(as<Symbol>(l.at(1)).name) = eval(l.at(2), env);
        if (head == "lambda") return cell(Lambda{as<List>(l.at(1)), l.at(2), env});
        if (head == "if") {
            x = truth(eval(l.at(1), env)) ? l.at(2) : l.size() > 3 ? l[3] : nil();
            continue;
        }
        if (head == "begin") {
            for (size_t i = 1; i + 1 < l.size(); ++i) eval(l[i], env);
            x = l.back();
            continue;
        }
        Ptr f = eval(l[0], env);
        List args;
        for (size_t i = 1; i < l.size(); ++i) args.push_back(eval(l[i], env));
        if (is<Prim>(f)) return as<Prim>(f)(args);
        if (is<Binary>(f)) return cell(as<Binary>(f)(as<double>(args.at(0)), as<double>(args.at(1))));
        const Lambda& fn = as<Lambda>(f);
        if (fn.params.size() != args.size()) throw std::runtime_error("wrong number of arguments");
        auto local = std::make_shared<Env>();
        local->parent = fn.env;
        for (size_t i = 0; i < args.size(); ++i) local->vars[as<Symbol>(fn.params[i]).name] = args[i];
        x = fn.body;
        env = local;
    }
}

// ------------------------------------------------------------------ primitives
double add(double a, double b) { return a + b; }
double sub(double a, double b) { return a - b; }
double mul(double a, double b) { return a * b; }
double dvd(double a, double b) { return a / b; }
double lt(double a, double b) { return a < b; }
double eq(double a, double b) { return a == b; }
Ptr car(const List& a) { return as<List>(a.at(0)).at(0); }
Ptr cdr(const List& a) {
    const List& l = as<List>(a.at(0));
    return cell(List(l.begin() + 1, l.end()));
}
Ptr cons(const List& a) {
    List r{a.at(0)};
    const List& tail = as<List>(a.at(1));
    r.insert(r.end(), tail.begin(), tail.end());
    return cell(r);
}
Ptr list(const List& a) { return cell(a); }
Ptr isnull(const List& a) { return cell(double(is<List>(a.at(0)) && as<List>(a.at(0)).empty())); }
Ptr display(const List& a) {
    for (const Ptr& x : a) print(x, std::cout) << ' ';
    std::cout << '\n';
    return nil();
}

EnvPtr globals() {
    auto env = std::make_shared<Env>();
    env->vars = {
        {"+", cell(add)}, {"-", cell(sub)}, {"*", cell(mul)}, {"/", cell(dvd)}, {"<", cell(lt)}, {"=", cell(eq)},
        {"car", cell(car)}, {"cdr", cell(cdr)}, {"cons", cell(cons)}, {"list", cell(list)},
        {"null?", cell(isnull)}, {"display", cell(display)},
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

void repl(EnvPtr env) {
    std::string src, line;
    long depth = 0;
    while (std::cout << (depth ? ".. " : ">> ") << std::flush, std::getline(std::cin, line)) {
        src += line + '\n';
        depth += std::count(line.begin(), line.end(), '(') - std::count(line.begin(), line.end(), ')');
        if (depth > 0) continue;
        try {
            std::istringstream in(src);
            run(in, env, true);
        } catch (std::out_of_range&) {
            std::cerr << "error: missing argument\n";
        } catch (std::exception& e) {
            std::cerr << "error: " << e.what() << '\n';
        }
        src.clear();
        depth = 0;
    }
}

#endif // LISP_H
