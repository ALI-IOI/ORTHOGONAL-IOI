// Simplifier.h - brings an expression to a tidy "sum of terms" form.
//
// Every expression is written as a sum of TERMS, and every term as
//      coefficient * factor1^power1 * factor2^power2 * ...
// Example:  (y / x^2) * 2x   ->   one term: coefficient 2, factors y^1, x^-1
//                            ->   printed as  2y/x
// Equal factors are merged by adding their powers, and equal terms are
// merged by adding their coefficients.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "Expr.h"

namespace ot {

struct Factor {
    Ptr base;
    double power;
};

struct Term {
    double coef = 1;
    std::map<std::string, Factor> factors;  // key = base->str()
};

using TermList = std::vector<Term>;

class Simplifier {
public:
    static Ptr simplify(const Ptr& e);

    static TermList toTerms(const Ptr& e);
    static Ptr fromTerms(const TermList& terms);
    static Ptr fromTerm(const Term& t);  // keeps the sign inside

private:
    static Term multiply(const Term& a, const Term& b);
    static TermList multiply(const TermList& a, const TermList& b);
    static TermList atom(const Ptr& base, double power);
    static TermList combineLikeTerms(const TermList& in);
};

}  // namespace ot
