// Solver.h - solving the orthogonal differential equation.
//
//  * ClosedFormSolver: if the equation is SEPARABLE,  dy/dx = P(x)·Q(y),
//    it integrates  ∫ dy/Q(y) = ∫ P(x) dx  with a small table of integrals
//    and then CHECKS the answer by differentiating it again.
//  * RK4Tracer: draws a trajectory numerically with the classical
//    4th-order Runge-Kutta method. Always used for the graph.
#pragma once
#include <string>
#include <vector>

#include "FamilyAnalyzer.h"
#include "Simplifier.h"
#include "Types.h"

namespace ot {

struct ClosedForm {
    bool found = false;
    std::vector<Step> steps;
    std::string result;    // e.g. "x^2 + 2y^2 = k"
};

class ClosedFormSolver {
public:
    ClosedForm solve(const FamilyAnalyzer& fa, const Box& view) const;

private:
    // ∫ term d(var); returns nullptr if the term is not in our table.
    Ptr integrate(const Term& t, int var) const;
    // Checks numerically that G(x, y) = k has slope -Gx/Gy equal to ortho.
    bool verify(const Ptr& G, const Ptr& ortho, const Box& view) const;
    std::string niceForm(const Ptr& G) const;
};

class RK4Tracer {
public:
    // bounds: stop when leaving this box.   h: step length (arc length).
    RK4Tracer(const FamilyAnalyzer& fa, const Box& bounds, double h);

    // Traces the trajectory through (x0, y0) in both directions.
    Polyline trace(double x0, double y0, double c0) const;

private:
    const FamilyAnalyzer& fa_;
    Box bounds_;
    double h_;
    // Direction at (x, y), turned to agree with the previous direction (px, py).
    bool field(double x, double y, double& c, double px, double py, double& dx, double& dy) const;
    void traceOneWay(double x0, double y0, double c0, double sign, std::vector<double>& out, std::vector<double>& cs,
                     bool& closed) const;
};

}  // namespace ot
