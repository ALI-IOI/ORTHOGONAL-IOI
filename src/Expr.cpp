// Expr.cpp - evaluation, differentiation and printing for every node type.
#include "Expr.h"

#include <cmath>
#include <cstdio>

namespace ot {

// ======================================================================
//  Number formatting
// ======================================================================
std::string formatNumber(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[64];
    double r = std::round(v);
    if (std::fabs(v - r) < 1e-9 * std::fmax(1.0, std::fabs(v)) && std::fabs(v) < 1e12) {
        std::snprintf(buf, sizeof buf, "%.0f", r);
        return buf;
    }
    // Try a small fraction p/q (q <= 12), e.g. 0.5 -> 1/2
    for (int q = 2; q <= 12; ++q) {
        double p = std::round(v * q);
        if (std::fabs(v * q - p) < 1e-9 * std::fmax(1.0, std::fabs(v * q))) {
            std::snprintf(buf, sizeof buf, "%.0f/%d", p, q);
            return buf;
        }
    }
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

// ======================================================================
//  Smart constructors (simple algebra while building)
// ======================================================================
namespace E {

bool isNum(const Ptr& p) {
    return p->kind() == Kind::Num && static_cast<const Num&>(*p).label.empty();
}
bool isNum(const Ptr& p, double v) {
    return isNum(p) && static_cast<const Num&>(*p).value == v;
}
static double val(const Ptr& p) { return static_cast<const Num&>(*p).value; }

Ptr num(double v) { return std::make_shared<Num>(v); }
Ptr var(int index, const std::string& name) { return std::make_shared<Var>(index, name); }

Ptr neg(Ptr a) {
    if (isNum(a)) return num(-val(a));
    if (a->kind() == Kind::Neg) return static_cast<const Neg&>(*a).a;  // -(-u) = u
    return std::make_shared<Neg>(a);
}

Ptr add(Ptr a, Ptr b) {
    if (isNum(a, 0)) return b;
    if (isNum(b, 0)) return a;
    if (isNum(a) && isNum(b)) return num(val(a) + val(b));
    if (b->kind() == Kind::Neg) return sub(a, static_cast<const Neg&>(*b).a);  // a + (-u) = a - u
    if (isNum(b) && val(b) < 0) return sub(a, num(-val(b)));
    if (a->kind() == Kind::Neg) return sub(b, static_cast<const Neg&>(*a).a);  // (-u) + b = b - u
    return std::make_shared<Add>(a, b);
}

Ptr sub(Ptr a, Ptr b) {
    if (isNum(b, 0)) return a;
    if (isNum(a, 0)) return neg(b);
    if (isNum(a) && isNum(b)) return num(val(a) - val(b));
    if (b->kind() == Kind::Neg) return add(a, static_cast<const Neg&>(*b).a);  // a - (-u) = a + u
    if (isNum(b) && val(b) < 0) return add(a, num(-val(b)));
    if (a->str() == b->str()) return num(0);                                  // u - u = 0
    return std::make_shared<Sub>(a, b);
}

Ptr mul(Ptr a, Ptr b) {
    if (isNum(a, 0) || isNum(b, 0)) return num(0);
    if (isNum(a, 1)) return b;
    if (isNum(b, 1)) return a;
    if (isNum(a, -1)) return neg(b);
    if (isNum(b, -1)) return neg(a);
    if (isNum(a) && isNum(b)) return num(val(a) * val(b));
    if (isNum(b) && !isNum(a)) return mul(b, a);                               // number first
    if (a->kind() == Kind::Neg) return neg(mul(static_cast<const Neg&>(*a).a, b));
    if (b->kind() == Kind::Neg) return neg(mul(a, static_cast<const Neg&>(*b).a));
    if (isNum(a) && b->kind() == Kind::Mul) {                                  // 2*(3*u) = 6*u
        auto& m = static_cast<const Mul&>(*b);
        if (isNum(m.l)) return mul(num(val(a) * val(m.l)), m.r);
    }
    return std::make_shared<Mul>(a, b);
}

Ptr div(Ptr a, Ptr b) {
    if (isNum(a, 0)) return num(0);
    if (isNum(b, 1)) return a;
    if (isNum(b, -1)) return neg(a);
    if (isNum(a) && isNum(b) && val(b) != 0) return num(val(a) / val(b));
    if (a->kind() == Kind::Neg) return neg(div(static_cast<const Neg&>(*a).a, b));
    if (b->kind() == Kind::Neg) return neg(div(a, static_cast<const Neg&>(*b).a));
    if (a->str() == b->str()) return num(1);                                  // u/u = 1
    return std::make_shared<Div>(a, b);
}

Ptr pow(Ptr a, Ptr b) {
    if (isNum(b, 0)) return num(1);
    if (isNum(b, 1)) return a;
    if (isNum(a, 1)) return num(1);
    if (isNum(a, 0) && isNum(b) && val(b) > 0) return num(0);
    if (isNum(a) && isNum(b)) {
        double r = std::pow(val(a), val(b));
        if (std::isfinite(r)) return num(r);
    }
    if (a->kind() == Kind::Num && static_cast<const Num&>(*a).label == "e")  // e^u = exp(u)
        return func("exp", b);
    if (a->kind() == Kind::Pow && isNum(b)) {                                // (u^m)^n = u^(m n)
        auto& p = static_cast<const Pow&>(*a);
        double n = val(b);
        if (isNum(p.r) && n == std::round(n) && val(p.r) == std::round(val(p.r)))
            return pow(p.l, num(val(p.r) * n));
    }
    return std::make_shared<Pow>(a, b);
}

Ptr func(const std::string& nameIn, Ptr a) {
    std::string name = (nameIn == "log") ? "ln" : nameIn;  // log means natural log
    if (name == "exp" && isNum(a, 0)) return num(1);
    if (name == "ln" && a->kind() == Kind::Num && static_cast<const Num&>(*a).label == "e") return num(1);
    if (name == "ln" && a->kind() == Kind::Func && static_cast<const Func&>(*a).name == "exp")
        return static_cast<const Func&>(*a).a;               // ln(e^u) = u
    auto f = std::make_shared<Func>(name, a);
    if (isNum(a)) {                                          // fold only "nice" results, e.g. sin(0) = 0
        double r = f->eval(Env());
        if (std::isfinite(r) && std::fabs(r - std::round(r)) < 1e-12) return num(std::round(r));
    }
    return f;
}
}  // namespace E

using namespace E;

// ======================================================================
//  Num
// ======================================================================
Ptr Num::derive(int) const { return num(0); }
std::string Num::str() const { return label.empty() ? formatNumber(value) : label; }
int Num::prec() const {
    if (!label.empty()) return P_ATOM;
    if (value < 0) return P_NEG;
    if (formatNumber(value).find('/') != std::string::npos) return P_MUL;
    return P_ATOM;
}
Ptr Num::substitute(int, const Ptr&) const { return std::make_shared<Num>(value, label); }

// ======================================================================
//  Var
// ======================================================================
Ptr Var::derive(int v) const { return num(v == index ? 1 : 0); }
Ptr Var::substitute(int v, const Ptr& with) const {
    return v == index ? with : var(index, name);
}

// ======================================================================
//  Neg
// ======================================================================
Ptr Neg::derive(int v) const { return neg(a->derive(v)); }
std::string Neg::str() const {
    std::string s = a->str();
    if (a->prec() == P_ADD || a->prec() == P_NEG) s = "(" + s + ")";  // -(a+b), -(-a)
    return "-" + s;
}
Ptr Neg::substitute(int v, const Ptr& w) const { return neg(a->substitute(v, w)); }

// ======================================================================
//  Binary operators
// ======================================================================
// Put brackets only where they are needed.
std::string Binary::str() const {
    std::string ls = l->str(), rs = r->str();
    if (l->prec() < prec()) ls = "(" + ls + ")";
    bool rightParen = r->prec() < prec() ||
                      (r->prec() == prec() && (kind() == Kind::Sub || kind() == Kind::Div)) ||
                      r->prec() == P_NEG;
    if (rightParen) rs = "(" + rs + ")";
    return ls + symbol() + rs;
}

// Sum rule: (u + v)' = u' + v'
Ptr Add::derive(int v) const { return add(l->derive(v), r->derive(v)); }
Ptr Add::substitute(int v, const Ptr& w) const { return add(l->substitute(v, w), r->substitute(v, w)); }

Ptr Sub::derive(int v) const { return sub(l->derive(v), r->derive(v)); }
Ptr Sub::substitute(int v, const Ptr& w) const { return sub(l->substitute(v, w), r->substitute(v, w)); }

// Product rule: (u v)' = u' v + u v'
Ptr Mul::derive(int v) const {
    return add(mul(l->derive(v), r), mul(l, r->derive(v)));
}
Ptr Mul::substitute(int v, const Ptr& w) const { return mul(l->substitute(v, w), r->substitute(v, w)); }

// Print 2x and cy instead of 2·x and c·y when that is still unambiguous.
std::string Mul::str() const {
    std::string ls = l->str(), rs = r->str();
    if (l->prec() < P_MUL || (l->kind() == Kind::Num && ls.find('/') != std::string::npos))
        ls = "(" + ls + ")";
    if (r->prec() <= P_MUL || r->prec() == P_NEG) rs = "(" + rs + ")";
    bool needDot = std::isdigit(static_cast<unsigned char>(rs[0])) || rs[0] == '.' ||
                   l->kind() == Kind::Pow || l->kind() == Kind::Func;
    if (needDot && ls.back() == ')' && rs[0] == '(') needDot = false;  // (a)(b)
    return ls + (needDot ? "·" : "") + rs;
}

// Quotient rule: (u/v)' = (u' v - u v') / v^2
Ptr Div::derive(int v) const {
    Ptr top = sub(mul(l->derive(v), r), mul(l, r->derive(v)));
    return div(top, pow(r, num(2)));
}
Ptr Div::substitute(int v, const Ptr& w) const { return div(l->substitute(v, w), r->substitute(v, w)); }

// Real power. For a negative base and an exponent p/q with q odd (e.g. a cube
// root) the real answer exists, so we compute it instead of returning NaN.
double Pow::eval(const Env& e) const {
    double b = l->eval(e), p = r->eval(e);
    if (b >= 0 || p == std::round(p)) return std::pow(b, p);
    for (int q = 3; q <= 9; q += 2) {
        double num = std::round(p * q);
        if (std::fabs(p * q - num) < 1e-9) {
            double mag = std::pow(-b, p);
            return (static_cast<long long>(num) % 2 == 0) ? mag : -mag;
        }
    }
    return NAN;
}

// Power rules:
//   u^n  (n constant)  ->  n u^(n-1) u'
//   a^u  (a constant)  ->  a^u ln(a) u'
//   u^v  (general)     ->  u^v (v' ln u + v u'/u)
Ptr Pow::derive(int v) const {
    bool baseVar = l->dependsOn(v), expVar = r->dependsOn(v);
    if (!baseVar && !expVar) return num(0);
    if (!expVar) return mul(mul(r, pow(l, sub(r, num(1)))), l->derive(v));
    if (!baseVar) return mul(mul(pow(l, r), func("ln", l)), r->derive(v));
    Ptr inner = add(mul(r->derive(v), func("ln", l)), div(mul(r, l->derive(v)), l));
    return mul(pow(l, r), inner);
}
std::string Pow::str() const {
    std::string ls = l->str(), rs = r->str();
    if (l->prec() <= P_POW) ls = "(" + ls + ")";
    if (r->prec() < P_ATOM) rs = "(" + rs + ")";
    return ls + "^" + rs;
}
Ptr Pow::substitute(int v, const Ptr& w) const { return pow(l->substitute(v, w), r->substitute(v, w)); }

// ======================================================================
//  Functions
// ======================================================================
const std::vector<std::string>& knownFunctions() {
    static const std::vector<std::string> names = {
        "asin", "acos", "atan", "sinh", "cosh", "tanh", "sqrt",
        "sin", "cos", "tan", "exp", "abs", "ln", "log"};
    return names;
}

double Func::eval(const Env& env) const {
    double u = a->eval(env);
    if (name == "sin") return std::sin(u);
    if (name == "cos") return std::cos(u);
    if (name == "tan") return std::tan(u);
    if (name == "exp") return std::exp(u);
    if (name == "ln") return std::log(u);
    if (name == "sqrt") return std::sqrt(u);
    if (name == "abs") return std::fabs(u);
    if (name == "asin") return std::asin(u);
    if (name == "acos") return std::acos(u);
    if (name == "atan") return std::atan(u);
    if (name == "sinh") return std::sinh(u);
    if (name == "cosh") return std::cosh(u);
    if (name == "tanh") return std::tanh(u);
    return NAN;
}

// Chain rule: f(u)' = f'(u) * u'
Ptr Func::derive(int v) const {
    Ptr du = a->derive(v);
    if (isNum(du, 0)) return num(0);
    Ptr outer;
    if (name == "sin") outer = func("cos", a);
    else if (name == "cos") outer = neg(func("sin", a));
    else if (name == "tan") outer = div(num(1), pow(func("cos", a), num(2)));
    else if (name == "exp") outer = func("exp", a);
    else if (name == "ln") outer = div(num(1), a);
    else if (name == "sqrt") outer = div(num(1), mul(num(2), func("sqrt", a)));
    else if (name == "abs") outer = div(a, func("abs", a));
    else if (name == "asin") outer = div(num(1), func("sqrt", sub(num(1), pow(a, num(2)))));
    else if (name == "acos") outer = neg(div(num(1), func("sqrt", sub(num(1), pow(a, num(2))))));
    else if (name == "atan") outer = div(num(1), add(num(1), pow(a, num(2))));
    else if (name == "sinh") outer = func("cosh", a);
    else if (name == "cosh") outer = func("sinh", a);
    else if (name == "tanh") outer = sub(num(1), pow(func("tanh", a), num(2)));
    else outer = num(NAN);
    return mul(outer, du);
}

std::string Func::str() const {
    std::string s = a->str();
    if (name == "exp") return (a->prec() < P_ATOM) ? "e^(" + s + ")" : "e^" + s;
    if (name == "abs") return "|" + s + "|";
    if (name == "sqrt") return "√(" + s + ")";
    return name + "(" + s + ")";
}
Ptr Func::substitute(int v, const Ptr& w) const { return func(name, a->substitute(v, w)); }

}  // namespace ot
