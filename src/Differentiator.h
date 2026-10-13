// Differentiator.h - partial derivatives of an expression tree.
//
// The real work is done by the virtual derive() function of each node
// (sum rule, product rule, quotient rule, power rule, chain rule; see
// Expr.cpp).  This class only adds the clean-up step afterwards.
#pragma once
#include "Expr.h"
#include "Simplifier.h"

namespace ot {

class Differentiator {
public:
    // d f / d var   (var = X, Y or C; the other variables are held constant)
    static Ptr partial(const Ptr& f, int var) { return Simplifier::simplify(f->derive(var)); }
};

}  // namespace ot
