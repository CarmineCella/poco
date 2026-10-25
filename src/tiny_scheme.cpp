// tiny_scheme.cpp -- minimal but safe and rich scheme dialect

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct Cell;
struct Env;
using Ptr = std::shared_ptr<Cell>;
using Frame = std::shared_ptr<Env>;
using List = std::vector<Ptr>;
using Prim = Ptr (*)(const List&);
struct Sym {
    std::string name;
};
struct Lambda {
    std::vector<std::string> params;
    Ptr body;
    Frame env;
};
struct Cell : std::variant<double, Sym, List, Prim, Lambda> {
    using variant::variant;
};
struct Env {
    std::unordered_map<std::string, Ptr> vars;
    Frame parent;
};
template<class T> Ptr make(T x) {
    return std::make_shared<Cell>(std::move(x));
}
template<class T> bool is(const Ptr& p) {
    return p && std::holds_alternative<T>(*p);
}
template<class T> const T& as(const Ptr& p) {
    if (!is<T>(p)) {
        throw std::runtime_error("type error");
    }
    return std::get<T>(*p);
}
Ptr nil() {
    return make(List{});
}
double number(const Ptr& p) {
    return as<double>(p);
}
void show(const Ptr& p, int depth=0) {
    if (depth>500) throw std::runtime_error("print too deeply nested");
    if (is<double>(p)) std::cout << as<double>(p);
    else if (is<Sym>(p)) std::cout << as<Sym>(p).name;
    else if (is<List>(p)) {
        std::cout << '(';
        const auto& a = as<List>(p);
        for (size_t i=0; i<a.size(); ++i) {
            if (i) std::cout << ' ';
            show(a[i],depth+1);
        }
        std::cout << ')';
    } else std::cout << "<procedure>";
}
std::string scan(std::istream& in) {
    std::string s;
    for (int c; (c=in.get())!=EOF;) {
        if (c==';') {
            if (!s.empty()) {
                in.unget();
                return s;
            }
            while ((c=in.get())!=EOF && c!='\n') {} continue;
        }
        if (std::isspace((unsigned char)c)) {
            if (!s.empty()) return s;
            continue;
        }
        if (c=='(' || c==')' || c=='\'') {
            if (!s.empty()) {
                in.unget();
                return s;
            }
            return std::string(1,char(c));
        } else s += char(c);
    }
    return s;
}
Ptr parse(std::istream& in, const std::string& t, int depth=0) {
    if (depth>500) throw std::runtime_error("source too deeply nested");
    if (t.empty()) throw std::runtime_error("unexpected EOF");
    if (t=="(") {
        List a;
        for (std::string u=scan(in); u!=")"; u=scan(in)) {
            if (u.empty()) {
                throw std::runtime_error("missing )");
            }
            a.push_back(parse(in,u,depth+1));
        }
        return make(std::move(a));
    }
    if (t==")") throw std::runtime_error("unexpected )");
    if (t=="'") return make(List{make(Sym{"quote"}),parse(in,scan(in),depth+1)});
    char* end=nullptr;
    errno=0;
    double x=std::strtod(t.c_str(),&end);
    if (end!=t.c_str() && *end=='\0') {
        if (errno==ERANGE || !std::isfinite(x)) {
            throw std::runtime_error("invalid number");
        }
        return make(x);
    }
    return make(Sym{t});
}
Ptr add(const List& a) {
    double v=0;
    for (auto& p:a) v+=number(p);
    return make(v);
}
Ptr mul(const List& a) {
    double v=1;
    for (auto& p:a) v*=number(p);
    return make(v);
}
Ptr sub(const List& a) {
    if (a.empty()) throw std::runtime_error("arity");
    double v=number(a[0]);
    if (a.size()==1) v=-v;
    for (size_t i=1; i<a.size(); ++i) {
        v-=number(a[i]);
    }
    return make(v);
}
Ptr divi(const List& a) {
    if (a.empty()) throw std::runtime_error("arity");
    double v=number(a[0]);
    if (a.size()==1) {
        if (!v) throw std::runtime_error("division by zero");
        v=1/v;
    }
    for (size_t i=1; i<a.size(); ++i) {
        double d=number(a[i]);
        if (!d) throw std::runtime_error("division by zero");
        v/=d;
    }
    return make(v);
}
Ptr less(const List& a) {
    if (a.size()!=2) throw std::runtime_error("arity");
    return make(double(number(a[0])<number(a[1])));
}
Ptr equal(const List& a) {
    if (a.size()!=2) throw std::runtime_error("arity");
    return make(double(number(a[0])==number(a[1])));
}
Ptr cons(const List& a) {
    if (a.size()!=2) throw std::runtime_error("arity");
    List r{a[0]};
    const auto& tail=as<List>(a[1]);
    r.insert(r.end(),tail.begin(),tail.end());
    return make(r);
}
Ptr car(const List& a) {
    if (a.size()!=1 || as<List>(a[0]).empty()) throw std::runtime_error("car: nonempty list expected");
    return as<List>(a[0])[0];
}
Ptr cdr(const List& a) {
    if (a.size()!=1 || as<List>(a[0]).empty()) throw std::runtime_error("cdr: nonempty list expected");
    const auto& l=as<List>(a[0]);
    return make(List(l.begin()+1,l.end()));
}
Ptr nullp(const List& a) {
    if (a.size()!=1) throw std::runtime_error("arity");
    return make(double(is<List>(a[0]) && as<List>(a[0]).empty()));
}
Ptr list(const List& a) {
    return make(a);
}
Ptr display(const List& a) {
    for (auto& p:a) {
        show(p);
        std::cout << ' ';
    }
    std::cout << '\n';
    return nil();
}
struct Machine {
    std::vector<std::weak_ptr<Env>> frames; Frame global; int depth=0;
    Frame frame(Frame parent= {}) {
        auto e=std::make_shared<Env>();
        e->parent=std::move(parent);
        frames.push_back(e);
        if (frames.size()>=4096) {
            frames.erase(std::remove_if(frames.begin(),frames.end(),
            [](const auto& w) {
                return w.expired();
            }),frames.end());
        }
        return e;
    }
    Machine() {
        global=frame();
        for (auto [name,fn] : std::vector<std::pair<std::string,Prim>> {
        {"+",add},{"-",sub},{"*",mul},{"/",divi},{"<",less},{"=",equal},
        {"cons",cons},{"car",car},{"cdr",cdr},{"null?",nullp},{"list",list},{"display",display}
    })
        global->vars[name]=make(fn);
    }
    ~Machine() {
        for (auto& w:frames) if (auto e=w.lock()) {
                e->vars.clear();
                e->parent.reset();
            }
    }
    Ptr eval(Ptr x, Frame e) {
        if (++depth>1000) {
            --depth;
            throw std::runtime_error("recursion too deep");
        }
        struct Guard {
            int& n;
            ~Guard() {
                --n;
            }
        } guard{depth};
        for (;;) {
            if (is<Sym>(x)) {
                const auto& name=as<Sym>(x).name;
                for (Frame f=e; f; f=f->parent) if (auto it=f->vars.find(name); it!=f->vars.end()) return it->second;
                throw std::runtime_error("unknown variable: "+name);
            }
            if (!is<List>(x) || as<List>(x).empty()) return x;
            const auto& a=as<List>(x);
            auto name=is<Sym>(a[0]) ? as<Sym>(a[0]).name : "";
            if (name=="quote") {
                if (a.size()!=2) throw std::runtime_error("quote: arity");
                return a[1];
            }
            if (name=="if") {
                if (a.size()!=3 && a.size()!=4) throw std::runtime_error("if: arity");
                x=number(eval(a[1],e))!=0 ? a[2] : a.size()==4 ? a[3] : nil();
                continue;
            }
            if (name=="begin") {
                for (size_t i=1; i+1<a.size(); ++i) eval(a[i],e);
                x=a.size()>1 ? a.back() : nil();
                continue;
            }
            if (name=="define" || name=="set!") {
                if (a.size()!=3) throw std::runtime_error("assignment: arity");
                std::string key=as<Sym>(a[1]).name;
                Ptr value=eval(a[2],e);
                if (name=="define") {
                    e->vars[key]=value;
                    return value;
                }
                for (Frame f=e; f; f=f->parent) if (auto it=f->vars.find(key); it!=f->vars.end()) return it->second=value;
                throw std::runtime_error("unknown variable: "+key);
            }
            if (name=="lambda") {
                if (a.size()!=3) throw std::runtime_error("lambda: arity");
                std::vector<std::string> params;
                for (const auto& p:as<List>(a[1])) {
                    std::string v=as<Sym>(p).name;
                    if (std::find(params.begin(),params.end(),v)!=params.end()) throw std::runtime_error("duplicate parameter");
                    params.push_back(v);
                }
                return make(Lambda{std::move(params),a[2],e});
            }
            Ptr fn=eval(a[0],e);
            List args;
            for (size_t i=1; i<a.size(); ++i) args.push_back(eval(a[i],e));
            if (is<Prim>(fn)) return as<Prim>(fn)(args);
            const auto& f=as<Lambda>(fn);
            if (f.params.size()!=args.size()) throw std::runtime_error("lambda: arity");
            Frame local=frame(f.env);
            for (size_t i=0; i<args.size(); ++i) {
                local->vars[f.params[i]]=args[i];
            }
            x=f.body;
            e=std::move(local);
        }
    }
    void run(std::istream& in, bool echo) {
        while (true) {
            std::string t=scan(in);
            if (t.empty()) break;
            Ptr value=eval(parse(in,t),global);
            if (echo) {
                show(value);
                std::cout << '\n';
            }
        }
    }
};
int main(int argc,char** argv) {
    try {
        Machine m;
        if (argc>1) {
            std::ifstream in(argv[1]);
            if (!in) throw std::runtime_error("cannot open file");
            m.run(in,false);
        } else m.run(std::cin,true);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
