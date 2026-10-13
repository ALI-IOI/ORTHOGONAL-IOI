// FamilyAnalyzer.cpp
#include "FamilyAnalyzer.h"

#include <cmath>

#include "Differentiator.h"
#include "Simplifier.h"

namespace ot {

using namespace E;

namespace {
// Put brackets around a sum so that "(2x + 1)·dy/dx" reads correctly.
std::string wrap(const Ptr& p) {
    std::string s = p->str();
    return (p->prec() <= P_ADD || p->prec() == P_NEG) ? "(" + s + ")" : s;
}
bool isZero(const Ptr& p) { return isNum(p, 0); }
}  // namespace

FamilyAnalyzer::FamilyAnalyzer(const Equation& eq, bool isExplicit, char paramName)
    : paramName(1, paramName), isExplicit_(isExplicit) {
    if (isExplicit) {
        // y = f(x, c): the left side must be exactly y.
        if (eq.hasEquals && eq.lhsText != "y")
            throw InputError("For the explicit type the left side must be just 'y' (you typed '" + eq.lhsText +
                             "'). Choose the implicit type F(x, y, c) = 0 for other forms.");
        explicitRhs_ = eq.hasEquals ? eq.rhs : eq.lhs;
        if (explicitRhs_->dependsOn(Y))
            throw InputError("In the explicit type y = f(x, " + this->paramName +
                             ") the right side must not contain y. Choose the implicit type instead.");
        F = sub(var(Y, "y"), explicitRhs_);
    } else {
        if (!eq.hasEquals)
            throw InputError("An implicit equation needs an '=' sign, for example x^2 + y^2 = " + this->paramName +
                             ". (For F(x, y, " + this->paramName + ") = 0 type '= 0' at the end.)");
        F = sub(eq.lhs, eq.rhs);
    }
    if (!F->dependsOn(C))
        throw InputError("The equation does not contain the parameter '" + this->paramName +
                         "', so it is ONE curve, not a family of curves. Example of a family: y = " +
                         this->paramName + "x^2.");
    if (!F->dependsOn(X) && !F->dependsOn(Y))
        throw InputError("The equation contains neither x nor y, so it does not describe curves in the xy-plane.");
    deriveSymbolically(eq);
}

void FamilyAnalyzer::deriveSymbolically(const Equation& eq) {
    const std::string& c = paramName;
    Fx = Differentiator::partial(F, X);
    Fy = Differentiator::partial(F, Y);
    Fc = Differentiator::partial(F, C);
    if (isZero(Fc))
        throw InputError("The parameter '" + c + "' cancels out of the equation, so all curves are the same. "
                         "A family needs a parameter that really changes the curve.");
    if (isZero(Fx) && isZero(Fy))
        throw InputError("After simplifying, the equation does not depend on x or y.");
    horizontalFamily = isZero(Fx);
    verticalFamily = isZero(Fy);

    // ---- Step 1: the family ----
    if (isExplicit_)
        steps.push_back({"Family of curves (parameter " + c + ")", "y = " + Simplifier::simplify(explicitRhs_)->str()});
    else
        steps.push_back({"Family of curves (parameter " + c + ")",
                         Simplifier::simplify(eq.lhs)->str() + " = " + Simplifier::simplify(eq.rhs)->str()});

    // ---- Step 2: differentiate with respect to x ----
    if (isExplicit_) {
        slopeWithC = Differentiator::partial(explicitRhs_, X);
        steps.push_back({"Differentiate with respect to x", "dy/dx = " + slopeWithC->str()});
    } else if (verticalFamily) {
        steps.push_back({"Differentiate with respect to x",
                         "∂F/∂y = 0, so every curve is a vertical line x = constant (dx/dy = 0)."});
    } else {
        slopeWithC = Simplifier::simplify(neg(div(Fx, Fy)));
        steps.push_back({"Differentiate with respect to x (implicit differentiation)",
                         wrap(Fx) + " + " + wrap(Fy) + "·dy/dx = 0   ⇒   dy/dx = " + slopeWithC->str()});
    }

    // ---- Step 3: eliminate the parameter ----
    linearInC = !Fc->dependsOn(C);
    if (linearInC) {
        // F = A(x,y) + c·B(x,y) with B = ∂F/∂c, so c = -A/B
        Ptr A = Simplifier::simplify(F->substitute(C, num(0)));
        cExpr = Simplifier::simplify(neg(div(A, Fc)));
        if (verticalFamily) {
            steps.push_back({"Eliminate " + c, c + " = " + cExpr->str() + "   (no slope needed: the curves are vertical lines)"});
        } else {
            slope = Simplifier::simplify(slopeWithC->substitute(C, cExpr));
            steps.push_back({"Eliminate " + c + " → differential equation of the family",
                             "From the family: " + c + " = " + cExpr->str() + "   ⇒   dy/dx = " + slope->str()});
        }
    } else {
        std::string s = slopeWithC ? slopeWithC->str() : "(vertical)";
        steps.push_back({"Eliminate " + c,
                         c + " appears non-linearly, so it cannot be isolated by simple algebra. The program finds " + c +
                             " numerically at every point from F(x, y, " + c + ") = 0 (Newton's method) and uses dy/dx = " + s + "."});
    }

    // ---- Step 4: orthogonal differential equation ----
    const std::string title = "Orthogonal trajectories: replace dy/dx by −1/(dy/dx)";
    if (horizontalFamily) {
        steps.push_back({title, "dy/dx = 0 for the family, and −1/0 is undefined: the trajectories are vertical, dx/dy = 0."});
    } else if (verticalFamily) {
        ortho = num(0);
        steps.push_back({title, "The family is vertical (dx/dy = 0), so the trajectories are horizontal: dy/dx = 0."});
    } else if (linearInC) {
        ortho = Simplifier::simplify(div(num(-1), slope));
        steps.push_back({title, "dy/dx = −1/(" + slope->str() + ")   ⇒   dy/dx = " + ortho->str()});
    } else {
        Ptr o = Simplifier::simplify(div(Fy, Fx));
        steps.push_back({title, "dy/dx = −1/(" + slopeWithC->str() + ") = " + o->str() + "   (" + c + " from step 3)"});
    }
}

bool FamilyAnalyzer::solveC(double x, double y, double& c) const {
    Env env(x, y, c);
    if (linearInC) {
        c = cExpr->eval(env);
        return std::isfinite(c);
    }
    // Newton's method on g(c) = F(x, y, c):  c_new = c - g(c) / g'(c)
    for (int it = 0; it < 30; ++it) {
        env.v[C] = c;
        double g = F->eval(env), dg = Fc->eval(env);
        if (!std::isfinite(g) || !std::isfinite(dg) || std::fabs(dg) < 1e-14) return false;
        double step = g / dg;
        double limit = 1.0 + std::fabs(c);
        if (std::fabs(step) > limit) step = (step > 0 ? limit : -limit);  // damping
        c -= step;
        if (std::fabs(step) < 1e-12 * limit) return std::isfinite(c);
    }
    return false;
}

bool FamilyAnalyzer::orthoDirection(double x, double y, double& c, double& dx, double& dy) const {
    if (!solveC(x, y, c)) return false;
    Env env(x, y, c);
    double gx = Fx->eval(env), gy = Fy->eval(env);
    double n = std::hypot(gx, gy);
    if (!std::isfinite(n) || n < 1e-12) return false;  // singular point
    dx = gx / n;
    dy = gy / n;
    return true;
}

bool FamilyAnalyzer::familyDirection(double x, double y, double c, double& dx, double& dy) const {
    Env env(x, y, c);
    double gx = Fx->eval(env), gy = Fy->eval(env);
    double n = std::hypot(gx, gy);
    if (!std::isfinite(n) || n < 1e-12) return false;
    dx = gy / n;
    dy = -gx / n;
    return true;
}

}  // namespace ot
