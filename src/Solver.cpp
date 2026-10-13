// Solver.cpp
#include "Solver.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "Differentiator.h"

namespace ot {

using namespace E;

// ======================================================================
//  Closed-form solution of separable equations
// ======================================================================
namespace {
Ptr lnAbs(const Ptr& v) { return func("ln", func("abs", v)); }

// d(base)/d(var) if it is a non-zero constant (base is linear in var), else 0.
double linearCoefficient(const Ptr& base, int var) {
    Ptr d = Differentiator::partial(base, var);
    if (isNum(d)) return static_cast<const Num&>(*d).value;
    return 0;
}

std::string withDifferential(const Term& t, const char* d) {
    Ptr p = Simplifier::fromTerm(t);
    if (isNum(p, 1)) return d;
    if (isNum(p, -1)) return std::string("-") + d;
    std::string s = p->str();
    if (p->prec() <= P_ADD) s = "(" + s + ")";
    return s + " " + d;
}
}  // namespace

Ptr ClosedFormSolver::integrate(const Term& t, int var) const {
    Ptr v = var == X ? E::var(X, "x") : E::var(Y, "y");
    Ptr k = num(t.coef);
    if (t.factors.empty()) return mul(k, v);                       // ∫ k dv = k v
    if (t.factors.size() != 1) return nullptr;
    const Factor& f = t.factors.begin()->second;
    double n = f.power;
    const Ptr& base = f.base;

    if (base->kind() == Kind::Var && static_cast<const Var&>(*base).index == var) {
        if (n == -1) return mul(k, lnAbs(v));                       // ∫ k/v dv = k ln|v|
        return mul(div(k, num(n + 1)), pow(v, num(n + 1)));         // ∫ k v^n dv = k v^(n+1)/(n+1)
    }
    if (base->kind() == Kind::Func) {
        auto& fn = static_cast<const Func&>(*base);
        double a = linearCoefficient(fn.a, var);
        if (a == 0) return nullptr;
        if (fn.name == "exp")                                      // ∫ k e^(n u) = k e^(n u) / (n a)
            return mul(div(k, num(n * a)), func("exp", Simplifier::simplify(mul(num(n), fn.a))));
        if (n == 1 && fn.name == "sin") return neg(mul(div(k, num(a)), func("cos", fn.a)));
        if (n == 1 && fn.name == "cos") return mul(div(k, num(a)), func("sin", fn.a));
        return nullptr;
    }
    double a = linearCoefficient(base, var);                       // base = a v + b
    if (a == 0) return nullptr;
    if (n == -1) return mul(div(k, num(a)), lnAbs(base));
    return mul(div(k, num((n + 1) * a)), pow(base, num(n + 1)));
}

bool ClosedFormSolver::verify(const Ptr& G, const Ptr& ortho, const Box& view) const {
    Ptr Gx = Differentiator::partial(G, X), Gy = Differentiator::partial(G, Y);
    int good = 0;
    for (int i = 1; i <= 80; ++i) {
        double x = view.xmin + halton(i, 2) * view.width();
        double y = view.ymin + halton(i, 3) * view.height();
        Env env(x, y, 0);
        double o = ortho->eval(env), s = -Gx->eval(env) / Gy->eval(env);
        if (!std::isfinite(o) || !std::isfinite(s) || std::fabs(o) > 1e6) continue;
        if (std::fabs(o - s) > 1e-6 * (1 + std::fabs(o))) return false;
        ++good;
    }
    return good >= 5;
}

std::string ClosedFormSolver::niceForm(const Ptr& G) const {
    TermList T = Simplifier::toTerms(G);

    // Case 1: only logarithms  a ln|y| + b ln|x| = K   ⇒   y = k x^(-b/a)
    double a = 0, b = 0;
    bool onlyLogs = !T.empty();
    for (const Term& t : T) {
        if (t.factors.size() != 1) { onlyLogs = false; break; }
        const Factor& f = t.factors.begin()->second;
        bool ok = f.power == 1 && f.base->kind() == Kind::Func && static_cast<const Func&>(*f.base).name == "ln";
        if (ok) {
            const Ptr& in = static_cast<const Func&>(*f.base).a;
            ok = in->kind() == Kind::Func && static_cast<const Func&>(*in).name == "abs" &&
                 static_cast<const Func&>(*in).a->kind() == Kind::Var;
            if (ok) {
                int idx = static_cast<const Var&>(*static_cast<const Func&>(*in).a).index;
                (idx == Y ? a : b) += t.coef;
            }
        }
        if (!ok) { onlyLogs = false; break; }
    }
    if (onlyLogs && a != 0 && b != 0) {
        double e = -b / a;
        std::string ex = formatNumber(e);
        if (ex.find('/') != std::string::npos || e < 0) ex = "(" + ex + ")";
        return G->str() + " = K   ⇒   y = k" + (e == 1 ? std::string("x") : "x^" + ex) + "   (k = ±e^(K/" +
               formatNumber(a) + "), any constant)";
    }

    // Case 2: clear fractions, x-terms first, first coefficient positive.
    long long L = 1;
    for (const Term& t : T) {
        bool found = false;
        for (long long q = 1; q <= 100 && !found; ++q)
            if (std::fabs(t.coef * q - std::round(t.coef * q)) < 1e-9) { L = std::lcm(L, q); found = true; }
        if (!found || L > 10000) { L = 1; break; }
    }
    std::stable_sort(T.begin(), T.end(), [](const Term& p, const Term& q) {
        auto rank = [](const Term& t) {
            bool hasX = false, hasY = false;
            for (auto& kv : t.factors) { hasX |= kv.second.base->dependsOn(X); hasY |= kv.second.base->dependsOn(Y); }
            return hasY ? (hasX ? 1 : 2) : 0;
        };
        return rank(p) < rank(q);
    });
    double sign = (!T.empty() && T[0].coef < 0) ? -1 : 1;
    for (Term& t : T) t.coef *= sign * static_cast<double>(L);
    return Simplifier::fromTerms(T)->str() + " = k";
}

ClosedForm ClosedFormSolver::solve(const FamilyAnalyzer& fa, const Box& view) const {
    ClosedForm r;
    const std::string rk4 = "Solved numerically with the 4th-order Runge–Kutta method (RK4).";
    if (fa.horizontalFamily) {
        r.found = true;
        r.result = "x = k";
        r.steps.push_back({"Solve", "dx/dy = 0   ⇒   x = k   (vertical lines)"});
        return r;
    }
    if (fa.verticalFamily) {
        r.found = true;
        r.result = "y = k";
        r.steps.push_back({"Solve", "dy/dx = 0   ⇒   y = k   (horizontal lines)"});
        return r;
    }
    if (!fa.linearInC) {
        r.steps.push_back({"Solve", "No closed form attempted because c was not eliminated symbolically. " + rk4});
        return r;
    }
    TermList T = Simplifier::toTerms(fa.ortho);
    if (T.size() != 1) {
        r.steps.push_back({"Solve", "The equation is not separable in a simple form P(x)·Q(y). " + rk4});
        return r;
    }
    // Split dy/dx = P(x)·Q(y)
    Term P, Q;
    P.coef = T[0].coef;
    for (auto& kv : T[0].factors) {
        bool dx = kv.second.base->dependsOn(X), dy = kv.second.base->dependsOn(Y);
        if (dx && dy) {
            r.steps.push_back({"Solve", "The equation is not separable (the factor " + kv.first +
                                            " contains both x and y). " + rk4});
            return r;
        }
        (dy ? Q : P).factors[kv.first] = kv.second;
    }
    Term invQ;  // 1/Q(y)
    for (auto& kv : Q.factors) invQ.factors[kv.first] = Factor{kv.second.base, -kv.second.power};

    r.steps.push_back({"Separate the variables", withDifferential(invQ, "dy") + " = " + withDifferential(P, "dx")});
    Ptr Iy = integrate(invQ, Y), Ix = integrate(P, X);
    if (!Iy || !Ix) {
        r.steps.push_back({"Solve", "Separable, but an integral is outside the program's small table of integrals. " + rk4});
        return r;
    }
    Iy = Simplifier::simplify(Iy);
    Ix = Simplifier::simplify(Ix);
    r.steps.push_back({"Integrate both sides", "∫ " + withDifferential(invQ, "dy") + " = ∫ " + withDifferential(P, "dx") +
                                                   "   ⇒   " + Iy->str() + " = " + Ix->str() + " + K"});
    Ptr G = Simplifier::simplify(sub(Iy, Ix));
    if (!verify(G, fa.ortho, view)) {
        r.steps.push_back({"Solve", "The integrated form did not pass the program's check, so it is not shown. " + rk4});
        return r;
    }
    r.found = true;
    r.result = niceForm(G);
    r.steps.push_back({"Orthogonal trajectories (closed form)", r.result});
    r.steps.push_back({"Check", "Differentiating the answer gives back the orthogonal equation at 80 test points "
                                "(checked by the program)."});
    return r;
}

// ======================================================================
//  RK4 tracing of one trajectory
// ======================================================================
RK4Tracer::RK4Tracer(const FamilyAnalyzer& fa, const Box& bounds, double h) : fa_(fa), bounds_(bounds), h_(h) {}

bool RK4Tracer::field(double x, double y, double& c, double px, double py, double& dx, double& dy) const {
    double cc = c;
    if (!fa_.orthoDirection(x, y, cc, dx, dy)) return false;
    if (dx * px + dy * py < 0) { dx = -dx; dy = -dy; }  // keep moving the same way
    return true;
}

void RK4Tracer::traceOneWay(double x, double y, double c, double sign, std::vector<double>& out, std::vector<double>& cs,
                            bool& closed) const {
    const double x0 = x, y0 = y;
    double px, py;
    if (!fa_.orthoDirection(x, y, c, px, py)) return;
    px *= sign; py *= sign;
    const int maxSteps = static_cast<int>(4 * (bounds_.width() + bounds_.height()) / h_);
    const double h = h_;
    for (int i = 0; i < maxSteps; ++i) {
        double k1x, k1y, k2x, k2y, k3x, k3y, k4x, k4y;
        // Classical RK4:  k1 = f(p), k2 = f(p + h/2 k1), k3 = f(p + h/2 k2), k4 = f(p + h k3)
        if (!field(x, y, c, px, py, k1x, k1y)) return;
        if (!field(x + h / 2 * k1x, y + h / 2 * k1y, c, k1x, k1y, k2x, k2y)) return;
        if (!field(x + h / 2 * k2x, y + h / 2 * k2y, c, k2x, k2y, k3x, k3y)) return;
        if (!field(x + h * k3x, y + h * k3y, c, k3x, k3y, k4x, k4y)) return;
        double nx = x + h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
        double ny = y + h / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
        double sx = nx - x, sy = ny - y, len = std::hypot(sx, sy);
        if (!std::isfinite(len) || len < 1e-15) return;
        if ((sx * px + sy * py) / len < 0.77) return;  // turned more than ~40°: singular point
        double cNew = c;
        if (!fa_.linearInC && !fa_.solveC(nx, ny, cNew)) return;
        c = cNew;
        x = nx; y = ny;
        px = sx / len; py = sy / len;
        out.push_back(x); out.push_back(y);
        cs.push_back(c);
        if (!bounds_.contains(x, y)) return;
        if (i > 20 && std::hypot(x - x0, y - y0) < 0.8 * h) {  // came back to the start: closed curve
            out.push_back(x0); out.push_back(y0);
            cs.push_back(cs.empty() ? c : cs.front());
            closed = true;
            return;
        }
    }
}

Polyline RK4Tracer::trace(double x0, double y0, double c0) const {
    Polyline p;
    std::vector<double> fwd, back, cf, cb;
    bool closed = false, dummy = false;
    double cStart = c0;
    fa_.solveC(x0, y0, cStart);
    traceOneWay(x0, y0, c0, +1, fwd, cf, closed);
    if (!closed) traceOneWay(x0, y0, c0, -1, back, cb, dummy);
    for (size_t i = back.size(); i >= 2; i -= 2) p.add(back[i - 2], back[i - 1]);  // backward part, reversed
    p.add(x0, y0);
    p.pts.insert(p.pts.end(), fwd.begin(), fwd.end());
    p.closed = closed;
    if (!fa_.linearInC) {  // remember c along the curve (used by the orthogonality check)
        p.cs.assign(cb.rbegin(), cb.rend());
        p.cs.push_back(cStart);
        p.cs.insert(p.cs.end(), cf.begin(), cf.end());
    }
    return p;
}

}  // namespace ot
