// #include <valarray>
// #include <vector>
// #include <string>
// #include <unordered_map>
// #include <functional>
// #include <sstream>
// #include <fstream>
// #include <iostream>
// #include <cmath>
// #include <stdexcept>
// #include <cstdlib>
// #include <stack>

// using A = std::valarray<double>;
// using Words = std::vector<std::string>;
// using Program = std::vector<Words>;
// using Env = std::unordered_map<std::string, A>;
// using Op = std::function<A(const std::vector<A>&)>;
// std::unordered_map<std::string, Op> ops;
// std::unordered_map<std::string, std::pair<Words, Program>> procs;

// A get(const std::string& s, const Env& env) {
//     char* end = nullptr;
//     double x = std::strtod(s.c_str(), &end);
//     if (end != s.c_str() && !*end) return A{x};
//     auto it = env.find(s);
//     if (it == env.end()) throw std::runtime_error("unknown variable: " + s);
//     return it->second;
// }

// void same(A& x, A& y) {
//     if (!x.size() || !y.size()) throw std::runtime_error("empty array");
//     if (x.size() != y.size()) {
//         if (x.size() == 1) x = A(x[0], y.size());
//         else if (y.size() == 1) y = A(y[0], x.size());
//         else throw std::runtime_error("incompatible sizes");
//     }
// }

// #define U(op, expr) ops[#op] = [](const std::vector<A>& v) { if (v.size() != 1) throw std::runtime_error("arity"); const A& x = v[0]; return A(expr); };
// #define B(op, expr) ops[#op] = [](const std::vector<A>& v) { if (v.size() != 2) throw std::runtime_error("arity"); A x = v[0], y = v[1]; same(x, y); return A(expr); };
// #define R(op, expr) ops[#op] = [](const std::vector<A>& v) { if (v.size() != 1 || !v[0].size()) throw std::runtime_error("arity/empty"); const A& x = v[0]; return A{expr}; };

// void init() {
//     U(sin, std::sin(x)) U(cos, std::cos(x)) U(tan, std::tan(x))
//     U(asin, std::asin(x)) U(acos, std::acos(x)) U(atan, std::atan(x))
//     U(sinh, std::sinh(x)) U(cosh, std::cosh(x)) U(tanh, std::tanh(x))
//     U(exp, std::exp(x)) U(log, std::log(x)) U(log10, std::log10(x))
//     U(log2, std::log(x) / std::log(2.0)) U(sqrt, std::sqrt(x))
//     U(abs, std::abs(x)) U(neg, -x)
//     U(floor, x.apply([](double v) { return std::floor(v); }))
//     U(ceil, x.apply([](double v) { return std::ceil(v); }))
//     U(round, x.apply([](double v) { return std::round(v); }))
//     U(trunc, x.apply([](double v) { return std::trunc(v); }))
//     B(add, x + y) B(sub, x - y) B(mul, x * y) B(div, x / y)
//     B(pow, std::pow(x, y)) B(atan2, std::atan2(x, y))
//     R(sum, x.sum()) R(mean, x.sum() / x.size()) R(min, x.min()) R(max, x.max())
//     R(norm, std::sqrt((x * x).sum())) R(rms, std::sqrt((x * x).sum() / x.size()))
//     R(size, double(x.size()))
//     R(prod, [&]() {
//         double r = 1;
//         for (double q : x) r *= q;
//         return r;
//     }())
//     R(variance, [&]() {
//         A d = x - x.sum() / x.size();
//         return (d * d).sum() / x.size();
//     }())
//     R(stddev, [&]() {
//         A d = x - x.sum() / x.size();
//         return std::sqrt((d * d).sum() / x.size());
//     }())
//     B(lt, [&]() {
//         A r(0.0, x.size());
//         for (size_t i = 0; i < r.size(); ++i) r[i] = x[i] < y[i];
//         return r;
//     }())
//     B(gt, [&]() {
//         A r(0.0, x.size());
//         for (size_t i = 0; i < r.size(); ++i) r[i] = x[i] > y[i];
//         return r;
//     }())
//     B(eq, [&]() {
//         A r(0.0, x.size());
//         for (size_t i = 0; i < r.size(); ++i) r[i] = x[i] == y[i];
//         return r;
//     }())
//     ops["dot"] = [](const std::vector<A>& v) {
//         if (v.size() != 2 || !v[0].size() || v[0].size() != v[1].size()) throw std::runtime_error("dot sizes");
//         return A{(v[0] * v[1]).sum()};
//     };
//     ops["shift"] = [](const std::vector<A>& v) {
//         if (v.size() != 2 || v[1].size() != 1) throw std::runtime_error("shift arity");
//         return A(v[0].shift(int(v[1][0])));
//     };
//     ops["cshift"] = [](const std::vector<A>& v) {
//         if (v.size() != 2 || v[1].size() != 1) throw std::runtime_error("cshift arity");
//         return A(v[0].cshift(int(v[1][0])));
//     };
// }

// A run(const Program& code, Env& env) {
//     std::stack<std::pair<size_t, size_t>> loop_stack;
//     for (size_t pc = 0; pc < code.size(); ++pc) {
//         const Words& w = code[pc];
//         if (w.empty()) continue;
//         const std::string& op = w[0];
//         if (op == "if") {
//             if (w.size() != 4) throw std::runtime_error("if arity");
//             A cond = get(w[1], env);
//             if (cond.size() != 1) throw std::runtime_error("if scalar");
//             if (cond[0]) {
//                 run(parse_block(w[2]), env);
//             } else {
//                 run(parse_block(w[3]), env);
//             }
//         } else if (op == "while") {
//             if (w.size() != 3) throw std::runtime_error("while arity");
//             loop_stack.push({pc, w.size()});
//             A cond = get(w[1], env);
//             if (cond.size() != 1) throw std::runtime_error("while scalar");
//             while (cond[0]) {
//                 run(parse_block(w[2]), env);
//                 cond = get(w[1], env);
//             }
//             loop_stack.pop();
//         } else if (op == "return") {
//             if (w.size() != 2) throw std::runtime_error("return arity");
//             return get(w[1], env);
//         } else if (op == "print") {
//             if (w.size() != 2) throw std::runtime_error("print arity");
//             A x = get(w[1], env);
//             std::cout << '[';
//             for (size_t k = 0; k < x.size(); ++k) std::cout << (k ? " " : "") << x[k];
//             std::cout << "]\n";
//         } else if (op == "set") {
//             if (w.size() != 3) throw std::runtime_error("set arity");
//             env[w[1]] = get(w[2], env);
//         } else if (op == "vec") {
//             A x(0.0, w.size() - 2);
//             for (size_t k = 2; k < w.size(); ++k) {
//                 A v = get(w[k], env);
//                 if (v.size() != 1) throw std::runtime_error("vec expects scalars");
//                 x[k - 2] = v[0];
//             }
//             env[w[1]] = std::move(x);
//         } else if (op == "get") {
//             if (w.size() != 4) throw std::runtime_error("get arity");
//             A a = get(w[2], env), ix = get(w[3], env);
//             if (ix.size() != 1 || !std::isfinite(ix[0]) || ix[0] < 0 || std::floor(ix[0]) != ix[0] || ix[0] >= a.size()) throw std::runtime_error("index");
//             env[w[1]] = A{a[size_t(ix[0])]};
//         } else if (op == "put") {
//             if (w.size() != 4) throw std::runtime_error("put arity");
//             A ix = get(w[2], env), val = get(w[3], env);
//             A& a = env.at(w[1]);
//             if (ix.size() != 1 || val.size() != 1 || !std::isfinite(ix[0]) || ix[0] < 0 || std::floor(ix[0]) != ix[0] || ix[0] >= a.size()) throw std::runtime_error("index");
//             a[size_t(ix[0])] = val[0];
//         } else if (op == "call") {
//             if (w.size() < 3) throw std::runtime_error("call arity");
//             auto& proc = procs.at(w[2]);
//             if (w.size() - 3 != proc.first.size()) throw std::runtime_error("call arity");
//             Env local;
//             for (size_t j = 0; j < proc.first.size(); ++j) local[proc.first[j]] = get(w[j + 3], env);
//             env[w[1]] = run(proc.second, local);
//         } else {
//             std::vector<A> args;
//             for (size_t j = 2; j < w.size(); ++j) args.push_back(get(w[j], env));
//             env[w.at(1)] = ops.at(op)(args);
//         }
//     }
//     return A{};
// }

// Program parse_block(const std::string& block) {
//     Program program;
//     std::istringstream in(block);
//     std::string line;
//     while (std::getline(in, line)) {
//         std::istringstream line_in(line);
//         Words w;
//         std::string word;
//         while (line_in >> word) w.push_back(word);
//         if (!w.empty()) program.push_back(std::move(w));
//     }
//     return program;
// }

// void load(std::istream& input) {
//     Program program;
//     std::string current;
//     Words params;
//     for (std::string line; std::getline(input, line);) {
//         if (auto comment = line.find('#'); comment != std::string::npos) line.erase(comment);
//         std::istringstream in(line);
//         Words w;
//         for (std::string word; in >> word;) w.push_back(word);
//         if (w.empty()) continue;
//         if (w[0] == "proc") {
//             if (!current.empty() || w.size() < 2) throw std::runtime_error("bad proc");
//             current = w[1];
//             params.assign(w.begin() + 2, w.end());
//             procs[current] = {params, {}};
//         } else if (w[0] == "end") {
//             if (current.empty()) throw std::runtime_error("unexpected end");
//             current.clear();
//         } else if (current.empty()) program.push_back(std::move(w));
//         else procs.at(current).second.push_back(std::move(w));
//     }
//     if (!current.empty()) throw std::runtime_error("missing end");
//     Env env;
//     run(program, env);
// }

// int main(int argc, char** argv) {
//     try {
//         init();
//         if (argc > 1) {
//             std::ifstream in(argv[1]);
//             if (!in) throw std::runtime_error("file");
//             load(in);
//         } else load(std::cin);
//     } catch (const std::exception& e) {
//         std::cerr << "error: " << e.what() << '\n';
//         return 1;
//     }
// }



#include <valarray>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <cstdlib>

using A=std::valarray<double>;
using Words=std::vector<std::string>;
using Program=std::vector<Words>;
using Env=std::unordered_map<std::string,A>;
using Op=std::function<A(const std::vector<A>&)>;
std::unordered_map<std::string,Op> ops;
std::unordered_map<std::string,std::pair<Words,Program>> procs;
A get(const std::string& s,const Env& env) {
    char* end=nullptr;
    double x=std::strtod(s.c_str(),&end);
    if(end!=s.c_str() && !*end) return A{x};
    auto it=env.find(s);
    if(it==env.end())throw std::runtime_error("unknown variable: "+s);
    return it->second;
}
void same(A& x,A& y) {
    if(!x.size()||!y.size())throw std::runtime_error("empty array");
    if(x.size()!=y.size()) {
        if(x.size()==1)x=A(x[0],y.size());
        else if(y.size()==1)y=A(y[0],x.size());
        else throw std::runtime_error("incompatible sizes");
    }
}
#define U(op,expr) ops[#op]=[](const std::vector<A>&v){if(v.size()!=1)throw std::runtime_error("arity");const A& x=v[0];return A(expr);};
#define B(op,expr) ops[#op]=[](const std::vector<A>&v){if(v.size()!=2)throw std::runtime_error("arity");A x=v[0],y=v[1];same(x,y);return A(expr);};
#define R(op,expr) ops[#op]=[](const std::vector<A>&v){if(v.size()!=1 || !v[0].size())throw std::runtime_error("arity/empty");const A& x=v[0];return A{expr};};
void init() {
    U(sin,std::sin(x)) U(cos,std::cos(x)) U(tan,std::tan(x))
    U(asin,std::asin(x)) U(acos,std::acos(x)) U(atan,std::atan(x))
    U(sinh,std::sinh(x)) U(cosh,std::cosh(x)) U(tanh,std::tanh(x))
    U(exp,std::exp(x)) U(log,std::log(x)) U(log10,std::log10(x))
    U(log2,std::log(x)/std::log(2.0)) U(sqrt,std::sqrt(x))
    U(abs,std::abs(x)) U(neg,-x)
    U(floor,x.apply([](double v) {
        return std::floor(v);
    }))
    U(ceil,x.apply([](double v) {
        return std::ceil(v);
    }))
    U(round,x.apply([](double v) {
        return std::round(v);
    }))
    U(trunc,x.apply([](double v) {
        return std::trunc(v);
    }))
    B(add,x+y) B(sub,x-y) B(mul,x*y) B(div,x/y)
    B(pow,std::pow(x,y)) B(atan2,std::atan2(x,y))
    R(sum,x.sum()) R(mean,x.sum()/x.size()) R(min,x.min()) R(max,x.max())
    R(norm,std::sqrt((x*x).sum())) R(rms,std::sqrt((x*x).sum()/x.size()))
    R(size,double(x.size()))
    R(prod,[&]() {
        double r=1;
        for(double q:x)r*=q;
        return r;
    }
    ())
    R(variance,[&]() {
        A d=x-x.sum()/x.size();
        return (d*d).sum()/x.size();
    }
    ())
    R(stddev,[&]() {
        A d=x-x.sum()/x.size();
        return std::sqrt((d*d).sum()/x.size());
    }
    ())
    B(lt,[&]() {
        A r(0.0,x.size());
        for(size_t i=0; i<r.size(); ++i)r[i]=x[i]<y[i]; return r;
    }
    ())
    B(gt,[&]() {
        A r(0.0,x.size());
        for(size_t i=0; i<r.size(); ++i)r[i]=x[i]>y[i];
        return r;
    }
    ())
    B(eq,[&]() {
        A r(0.0,x.size());
        for(size_t i=0; i<r.size(); ++i)r[i]=x[i]==y[i];
        return r;
    }
    ())
    ops["dot"]=[](const std::vector<A>&v) {
        if(v.size()!=2||!v[0].size()||v[0].size()!=v[1].size())throw std::runtime_error("dot sizes");
        return A{(v[0]*v[1]).sum()};
    };
    ops["shift"]=[](const std::vector<A>&v) {
        if(v.size()!=2||v[1].size()!=1)throw std::runtime_error("shift arity");
        return A(v[0].shift(int(v[1][0])));
    };
    ops["cshift"]=[](const std::vector<A>&v) {
        if(v.size()!=2||v[1].size()!=1)throw std::runtime_error("cshift arity");
        return A(v[0].cshift(int(v[1][0])));
    };
}
A run(const Program& code,Env& env) {
    std::unordered_map<std::string,size_t> labels;
    for(size_t i=0; i<code.size(); ++i)
        if(code[i].size()==1 && code[i][0].back()==':')
            labels[code[i][0].substr(0,code[i][0].size()-1)]=i;
    for(size_t pc=0; pc<code.size(); ++pc) {
        const Words& w=code[pc];
        if(w.empty() || w[0].back()==':')continue;
        const std::string& op=w[0];
        if(op=="goto") {
            if(w.size()!=2)throw std::runtime_error("goto arity");
            pc=labels.at(w[1]);
        } else if(op=="if") {
            if(w.size()!=3)throw std::runtime_error("if arity");
            A c=get(w[1],env);
            if(c.size()!=1)throw std::runtime_error("if scalar");
            if(c[0])pc=labels.at(w[2]);
        } else if(op=="return") {
            if(w.size()!=2)throw std::runtime_error("return arity");
            return get(w[1],env);
        } else if(op=="print") {
            if(w.size()!=2)throw std::runtime_error("print arity");
            A x=get(w[1],env);
            std::cout<<'[';
            for(size_t k=0; k<x.size(); ++k)std::cout<<(k?" ":"")<<x[k];
            std::cout<<"]\n";
        } else if(op=="set") {
            if(w.size()!=3)throw std::runtime_error("set arity");
            env[w[1]]=get(w[2],env);
        } else if(op=="vec") {
            A x(0.0,w.size()-2);
            for(size_t k=2; k<w.size(); ++k) {
                A v=get(w[k],env);
                if(v.size()!=1)throw std::runtime_error("vec expects scalars");
                x[k-2]=v[0];
            }
            env[w[1]]=std::move(x);
        } else if(op=="get") {
            if(w.size()!=4)throw std::runtime_error("get arity");
            A a=get(w[2],env),ix=get(w[3],env);
            if(ix.size()!=1||!std::isfinite(ix[0])||ix[0]<0||std::floor(ix[0])!=ix[0]||ix[0]>=a.size())throw std::runtime_error("index");
            env[w[1]]=A{a[size_t(ix[0])]};
        } else if(op=="put") {
            if(w.size()!=4)throw std::runtime_error("put arity");
            A ix=get(w[2],env),val=get(w[3],env);
            A& a=env.at(w[1]);
            if(ix.size()!=1||val.size()!=1||!std::isfinite(ix[0])||ix[0]<0||std::floor(ix[0])!=ix[0]||ix[0]>=a.size())throw std::runtime_error("index");
            a[size_t(ix[0])]=val[0];
        } else if(op=="call") {
            if(w.size()<3)throw std::runtime_error("call arity");
            auto& proc=procs.at(w[2]);
            if(w.size()-3!=proc.first.size())throw std::runtime_error("call arity");
            Env local;
            for(size_t j=0; j<proc.first.size(); ++j)local[proc.first[j]]=get(w[j+3],env);
            env[w[1]]=run(proc.second,local);
        } else {
            std::vector<A> args;
            for(size_t j=2; j<w.size(); ++j)args.push_back(get(w[j],env));
            env[w.at(1)]=ops.at(op)(args);
        }
    }
    return A{};
}
void load(std::istream& input) {
    Program program;
    std::string current;
    Words params;
    for(std::string line; std::getline(input,line);) {
        if(auto comment=line.find('#'); comment!=std::string::npos) line.erase(comment);
        std::istringstream in(line);
        Words w;
        for(std::string word; in>>word;)w.push_back(word);
        if(w.empty())continue;
        if(w[0]=="proc") {
            if(!current.empty()||w.size()<2)throw std::runtime_error("bad proc");
            current=w[1];
            params.assign(w.begin()+2,w.end());
            procs[current]= {params,{}};
        } else if(w[0]=="end") {
            if(current.empty())throw std::runtime_error("unexpected end");
            current.clear();
        } else if(current.empty())program.push_back(std::move(w));
        else procs.at(current).second.push_back(std::move(w));
    }
    if(!current.empty())throw std::runtime_error("missing end");
    Env env;
    run(program,env);
}
int main(int argc,char**argv) {
    try {
        init();
        if(argc>1) {
            std::ifstream in(argv[1]);
            if(!in)throw std::runtime_error("file");
            load(in);
        } else load(std::cin);
    } catch(const std::exception& e) {
        std::cerr<<"error: "<<e.what()<<'\n';
        return 1;
    }
}
