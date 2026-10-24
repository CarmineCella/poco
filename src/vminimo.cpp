#include <valarray>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
using A = std::valarray<double>;
using Words = std::vector<std::string>;
std::vector<A> stack;
std::unordered_map<std::string,Words> defs;
std::unordered_map<std::string,std::function<void()>> ops;
A pop() {
    if(stack.empty()) throw std::runtime_error("stack underflow");
    A x=std::move(stack.back());
    stack.pop_back();
    return x;
}
double number(A x) {
    if(x.size()!=1) throw std::runtime_error("expected scalar");
    return x[0];
}
void same(A& x, A& y) {
    if(!x.size() || !y.size()) throw std::runtime_error("empty array");
    if(x.size()==y.size()) return;
    if(x.size()==1) x=A(x[0],y.size());
    else if(y.size()==1) y=A(y[0],x.size());
    else throw std::runtime_error("incompatible sizes");
}
#define U(op, expr) ops[#op]=[]{ A x=pop(); stack.push_back(A(expr)); };
#define B(op, expr) ops[#op]=[]{ A y=pop(),x=pop(); same(x,y); stack.push_back(A(expr)); };
#define R(op, expr) ops[#op]=[]{ A x=pop(); if(!x.size()) throw std::runtime_error("empty array"); stack.push_back(A{expr}); };
void init() {
    U(sin,std::sin(x)) U(cos,std::cos(x)) U(tan,std::tan(x)) U(asin,std::asin(x))
    U(acos,std::acos(x)) U(atan,std::atan(x)) U(sinh,std::sinh(x))
    U(cosh,std::cosh(x)) U(tanh,std::tanh(x)) U(exp,std::exp(x))
    U(log,std::log(x)) U(log10,std::log10(x)) U(log2,std::log(x)/std::log(2.0))
    U(sqrt,std::sqrt(x)) U(abs,std::abs(x)) U(neg,-x)
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
        double z=1;
        for(double v:x)z*=v;
        return z;
    }
    ())
    ops["stddev"]=[] { A x=pop(); if(!x.size()) throw std::runtime_error("empty array"); A d=x-x.sum()/x.size(); stack.push_back(A{std::sqrt((d*d).sum()/x.size())}); };
ops["variance"]=[] { A x=pop(); if(!x.size()) throw std::runtime_error("empty array"); A d=x-x.sum()/x.size(); stack.push_back(A{(d*d).sum()/x.size()}); };
ops["dot"]=[] { A y=pop(),x=pop(); if(!x.size() || x.size()!=y.size()) throw std::runtime_error("dot size"); stack.push_back(A{(x*y).sum()}); };
ops["dup"]=[] { A x=pop(); stack.push_back(x); stack.push_back(std::move(x)); };
    ops["drop"]=[] {pop();};
    ops["swap"]=[] {A y=pop(),x=pop(); stack.push_back(std::move(y)); stack.push_back(std::move(x));};
    ops["over"]=[] {A y=pop(),x=pop(); stack.push_back(x); stack.push_back(y); stack.push_back(std::move(x));};
    ops["print"]=[] {A x=pop(); std::cout << '['; for(size_t i=0; i<x.size(); ++i) std::cout << (i?" ":"") << x[i]; std::cout << "]\n";};
ops["vec"]=[] {double count=number(pop()); if(!std::isfinite(count)||count<0||std::floor(count)!=count||count>stack.size())throw std::runtime_error("vec count"); int n=int(count); A x(double(0),n); for(int i=n; i>0; --i) x[i-1]=number(pop()); stack.push_back(std::move(x));};
ops["get"]=[] {double idx=number(pop()); A x=pop(); if(!std::isfinite(idx)||idx<0||idx!=std::floor(idx)||idx>=x.size())throw std::runtime_error("index"); stack.push_back(A{x[size_t(idx)]});};
ops["put"]=[] {double value=number(pop()),idx=number(pop()); A x=pop(); if(!std::isfinite(idx)||idx<0||idx!=std::floor(idx)||idx>=x.size()) throw std::runtime_error("index"); x[size_t(idx)]=value; stack.push_back(std::move(x));};
ops["shift"]=[] {double d=number(pop()); if(!std::isfinite(d)||d!=std::floor(d)||std::abs(d)>2147483647)throw std::runtime_error("shift offset"); A x=pop(); stack.push_back(x.shift(int(d)));};
ops["cshift"]=[] {double d=number(pop()); if(!std::isfinite(d)||d!=std::floor(d)||std::abs(d)>2147483647)throw std::runtime_error("cshift offset"); A x=pop(); stack.push_back(x.cshift(int(d)));};
for(std::string name: {
                "gt","lt","eq"
            })
        ops[name]=[name] {A y=pop(),x=pop(); same(x,y); A r(0.0,x.size()); for(size_t i=0; i<r.size(); ++i)r[i]=name=="gt"?x[i]>y[i]:name=="lt"?x[i]<y[i]:x[i]==y[i]; stack.push_back(std::move(r));};
}
size_t skip(const Words& p,size_t i,bool use_else) {
    int depth=0;
    while(++i<p.size()) {
        if(p[i]=="if") ++depth;
        else if(p[i]=="then") {
            if(depth==0) return i;
            --depth;
        } else if(use_else && p[i]=="else" && depth==0) return i;
    }
    throw std::runtime_error("missing then");
}
void run(const Words& p) {
    std::vector<size_t> loops;
    for(size_t i=0; i<p.size(); ++i) {
        const std::string& w=p[i];
        char* end=nullptr;
        double n=std::strtod(w.c_str(),&end);
        if(end!=w.c_str() && !*end) stack.push_back(A{n});
        else if(w=="if") {
            if(!number(pop())) i=skip(p,i,true);
        } else if(w=="else") i=skip(p,i,false);
        else if(w=="then") {}
        else if(w=="begin") loops.push_back(i);
        else if(w=="until") {
            if(loops.empty())throw std::runtime_error("unexpected until");
            if(!number(pop())) i=loops.back();
            else loops.pop_back();
        } else if(auto it=ops.find(w); it!=ops.end())it->second();
        else if(auto it=defs.find(w); it!=defs.end())run(it->second);
        else throw std::runtime_error("unknown word: "+w);
    }
    if(!loops.empty())throw std::runtime_error("missing until");
}
void load(std::istream& in) {
    Words code;
    for(std::string w; in>>w;) {
        if(w==":") {
            std::string name;
            if(!(in>>name))throw std::runtime_error("missing name");
            Words body;
            while(in>>w && w!=";")body.push_back(w);
            if(w!=";")throw std::runtime_error("missing ;");
            defs[name]=std::move(body);
        } else code.push_back(w);
    }
    run(code);
}
int main(int argc,char** argv) {
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
