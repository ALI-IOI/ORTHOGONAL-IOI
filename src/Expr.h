// Expr.h - Expression tree for mathematical formulas.
//
// Every formula (for example  y - c*x^2 ) is stored as a TREE of nodes:
//
//            (-)
//           /   |
//          y   (*)
//              /  |
//             c  (^)
//                /  |
//               x   2
//
// Node is an ABSTRACT base class. Each kind of node (number, variable, +, -,
// *, /, ^, function) is a subclass that overrides the virtual functions
// eval(), derive() and str().  This is runtime POLYMORPHISM: the caller just
// writes node->derive(X) and the correct rule is chosen automatically.
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace ot {

// Index of each variable inside an Env.  x, y and the family parameter c.
enum VarIndex { X = 0, Y = 1, C = 2 };

// Values of x, y, c used when evaluating a tree.
struct Env {
    double v[3] = {0, 0, 0};
    Env() = default;
    Env(double x, double y, double c) { v[X] = x; v[Y] = y; v[C] = c; }
};

enum class Kind { Num, Var, Neg, Add, Sub, Mul, Div, Pow, Func };

// Precedence levels, used only for printing with the fewest brackets.
enum Prec { P_ADD = 1, P_MUL = 2, P_NEG = 3, P_POW = 4, P_ATOM = 5 };

class Node;
using Ptr = std::shared_ptr<const Node>;  // nodes are shared and never changed

class Node {
public:
    virtual ~Node() = default;
    virtual Kind kind() const = 0;
    virtual double eval(const Env& env) const = 0;   // numeric value
    virtual Ptr derive(int var) const = 0;           // partial derivative
    virtual std::string str() const = 0;             // readable text
    virtual int prec() const = 0;                    // precedence for printing
    virtual bool dependsOn(int var) const = 0;       // does it contain var?
    virtual Ptr substitute(int var, const Ptr& with) const = 0;  // replace var
};

// ---------- Leaf nodes ----------
class Num : public Node {
public:
    double value;
    std::string label;  // "pi" or "e" for named constants, empty otherwise
    explicit Num(double v, std::string lbl = "") : value(v), label(std::move(lbl)) {}
    Kind kind() const override { return Kind::Num; }
    double eval(const Env&) const override { return value; }
    Ptr derive(int) const override;
    std::string str() const override;
    int prec() const override;
    bool dependsOn(int) const override { return false; }
    Ptr substitute(int, const Ptr&) const override;
};

class Var : public Node {
public:
    int index;          // X, Y or C
    std::string name;   // what the user typed ("x", "y", "c", "k", ...)
    Var(int i, std::string n) : index(i), name(std::move(n)) {}
    Kind kind() const override { return Kind::Var; }
    double eval(const Env& env) const override { return env.v[index]; }
    Ptr derive(int var) const override;
    std::string str() const override { return name; }
    int prec() const override { return P_ATOM; }
    bool dependsOn(int var) const override { return var == index; }
    Ptr substitute(int var, const Ptr& with) const override;
};

// ---------- Unary minus ----------
class Neg : public Node {
public:
    Ptr a;
    explicit Neg(Ptr x) : a(std::move(x)) {}
    Kind kind() const override { return Kind::Neg; }
    double eval(const Env& env) const override { return -a->eval(env); }
    Ptr derive(int var) const override;
    std::string str() const override;
    int prec() const override { return P_NEG; }
    bool dependsOn(int var) const override { return a->dependsOn(var); }
    Ptr substitute(int var, const Ptr& with) const override;
};

// ---------- Binary operators: shared base class (inheritance) ----------
class Binary : public Node {
public:
    Ptr l, r;
    Binary(Ptr a, Ptr b) : l(std::move(a)), r(std::move(b)) {}
    bool dependsOn(int var) const override { return l->dependsOn(var) || r->dependsOn(var); }
    std::string str() const override;
protected:
    virtual const char* symbol() const = 0;
};

class Add : public Binary {
public:
    using Binary::Binary;
    Kind kind() const override { return Kind::Add; }
    double eval(const Env& e) const override { return l->eval(e) + r->eval(e); }
    Ptr derive(int var) const override;
    int prec() const override { return P_ADD; }
    Ptr substitute(int var, const Ptr& with) const override;
protected:
    const char* symbol() const override { return " + "; }
};

class Sub : public Binary {
public:
    using Binary::Binary;
    Kind kind() const override { return Kind::Sub; }
    double eval(const Env& e) const override { return l->eval(e) - r->eval(e); }
    Ptr derive(int var) const override;
    int prec() const override { return P_ADD; }
    Ptr substitute(int var, const Ptr& with) const override;
protected:
    const char* symbol() const override { return " - "; }
};

class Mul : public Binary {
public:
    using Binary::Binary;
    Kind kind() const override { return Kind::Mul; }
    double eval(const Env& e) const override { return l->eval(e) * r->eval(e); }
    Ptr derive(int var) const override;
    std::string str() const override;  // writes 2x instead of 2*x
    int prec() const override { return P_MUL; }
    Ptr substitute(int var, const Ptr& with) const override;
protected:
    const char* symbol() const override { return "·"; }
};

class Div : public Binary {
public:
    using Binary::Binary;
    Kind kind() const override { return Kind::Div; }
    double eval(const Env& e) const override { return l->eval(e) / r->eval(e); }
    Ptr derive(int var) const override;
    int prec() const override { return P_MUL; }
    Ptr substitute(int var, const Ptr& with) const override;
protected:
    const char* symbol() const override { return "/"; }
};

class Pow : public Binary {
public:
    using Binary::Binary;
    Kind kind() const override { return Kind::Pow; }
    double eval(const Env& e) const override;
    Ptr derive(int var) const override;
    std::string str() const override;
    int prec() const override { return P_POW; }
    Ptr substitute(int var, const Ptr& with) const override;
protected:
    const char* symbol() const override { return "^"; }
};

// ---------- Function call: sin(u), ln(u), exp(u), ... ----------
class Func : public Node {
public:
    std::string name;
    Ptr a;
    Func(std::string n, Ptr x) : name(std::move(n)), a(std::move(x)) {}
    Kind kind() const override { return Kind::Func; }
    double eval(const Env& env) const override;
    Ptr derive(int var) const override;
    std::string str() const override;
    int prec() const override { return name == "exp" ? P_POW : P_ATOM; }
    bool dependsOn(int var) const override { return a->dependsOn(var); }
    Ptr substitute(int var, const Ptr& with) const override;
};

// List of function names the program understands.
const std::vector<std::string>& knownFunctions();

// ---------- "Smart constructors" ----------
// They build a node but first apply easy rules such as 0 + a = a, 1 * a = a,
// a * 0 = 0 and fold numbers (2 * 3 = 6).  This keeps derivatives short.
namespace E {
Ptr num(double v);
Ptr var(int index, const std::string& name);
Ptr neg(Ptr a);
Ptr add(Ptr a, Ptr b);
Ptr sub(Ptr a, Ptr b);
Ptr mul(Ptr a, Ptr b);
Ptr div(Ptr a, Ptr b);
Ptr pow(Ptr a, Ptr b);
Ptr func(const std::string& name, Ptr a);
bool isNum(const Ptr& p, double v);   // is p exactly the number v?
bool isNum(const Ptr& p);             // is p a plain (unnamed) number?
}  // namespace E

// Turns a double into short text: 2, -3, 1/2, 0.125, ...
std::string formatNumber(double v);

}  // namespace ot
