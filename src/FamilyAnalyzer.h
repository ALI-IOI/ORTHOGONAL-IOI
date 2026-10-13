// FamilyAnalyzer.h - the mathematics of a one-parameter family of curves.
//
// Input : the parsed equation, e.g.  y = c x^2   or   x^2 + y^2 = c
// Output: (1) F(x, y, c) = 0 form,
//         (2) the differential equation of the family   dy/dx = m(x, y)
//             obtained by differentiating and ELIMINATING c,
//         (3) the orthogonal differential equation      dy/dx = -1 / m(x, y),
//         (4) numeric helpers used for drawing (direction fields).
#pragma once
#include <string>
#include <vector>

#include "Expr.h"
#include "Parser.h"
#include "Types.h"

namespace ot {

class FamilyAnalyzer {
public:
    // Throws InputError if the equation is not a usable family of curves.
    FamilyAnalyzer(const Equation& eq, bool isExplicit, char paramName);

    // ----- symbolic results -----
    Ptr F, Fx, Fy, Fc;        // F(x,y,c) and its partial derivatives
    bool linearInC = false;   // c can be isolated symbolically: F = A(x,y) + c*B(x,y)
    Ptr cExpr;                // c = cExpr(x, y)            (only if linearInC)
    Ptr slopeWithC;           // dy/dx, still containing c
    Ptr slope;                // dy/dx = m(x, y)            (only if linearInC)
    Ptr ortho;                // orthogonal: dy/dx = -1/m   (only if linearInC)
    bool horizontalFamily = false;  // Fx = 0 everywhere: curves are lines y = const
    bool verticalFamily = false;    // Fy = 0 everywhere: curves are lines x = const
    std::string paramName;
    std::vector<Step> steps;  // derivation text for the page

    // ----- numeric helpers -----
    double evalF(double x, double y, double c) const { return F->eval(Env(x, y, c)); }

    // Finds the value of c of the family member through (x, y).
    // c is used as the starting guess and receives the answer.
    bool solveC(double x, double y, double& c) const;

    // Unit direction of the ORTHOGONAL trajectory through (x, y): the gradient
    // (Fx, Fy) is perpendicular to the family curve F(x, y, c) = 0.
    bool orthoDirection(double x, double y, double& c, double& dx, double& dy) const;

    // Unit tangent of the family curve with parameter c at (x, y): (Fy, -Fx).
    bool familyDirection(double x, double y, double c, double& dx, double& dy) const;

private:
    bool isExplicit_;
    Ptr explicitRhs_;
    void deriveSymbolically(const Equation& eq);
};

}  // namespace ot
