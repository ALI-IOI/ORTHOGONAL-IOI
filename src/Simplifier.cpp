// Simplifier.cpp
#include "Simplifier.h"

#include <algorithm>
#include <cmath>

namespace ot {

namespace {
bool isInteger(double v) { return std::fabs(v - std::round(v)) < 1e-12; }

// Order of factors when printing: parameter first (cx^2), then x, then y, then the rest.
int rank(const Factor& f) {
    if (f.base->kind() == Kind::Var) {
        int i = static_cast<const Var&>(*f.base).index;
        return i == C ? 0 : i + 1;
    }
    if (f.base->kind() == Kind::Num) return 4;
    return 5;
}
std::vector<Factor> sortedFactors(const Term& t) {
    std::vector<Factor> v;
    for (auto& kv : t.factors) v.push_back(kv.second);
    std::stable_sort(v.begin(), v.end(), [](const Factor& a, const Factor& b) { return rank(a) < rank(b); });
    return v;
}
// Writes |c| as p/q with small q when possible.
void asFraction(double c, double& p, double& q) {
    for (int d = 1; d <= 100; ++d) {
        double n = std::round(c * d);
        if (std::fabs(c * d - n) < 1e-9 * std::fmax(1.0, std::fabs(c * d))) { p = n; q = d; return; }
    }
    p = c; q = 1;
}
}  // namespace

TermList Simplifier::atom(const Ptr& base, double power) {
    Term t;
    t.factors[base->str()] = Factor{base, power};
    return {t};
}

Term Simplifier::multiply(const Term& a, const Term& b) {
    Term r = a;
    r.coef *= b.coef;
    for (auto& kv : b.factors) {
        auto it = r.factors.find(kv.first);
        if (it == r.factors.end()) r.factors[kv.first] = kv.second;
        else {
            it->second.power += kv.second.power;           // x^a * x^b = x^(a+b)
            if (std::fabs(it->second.power) < 1e-12) r.factors.erase(it);
        }
    }
    return r;
}

TermList Simplifier::multiply(const TermList& a, const TermList& b) {
    TermList out;
    for (const Term& s : a)
        for (const Term& t : b) out.push_back(multiply(s, t));  // distribute
    return combineLikeTerms(out);
}

TermList Simplifier::combineLikeTerms(const TermList& in) {
    TermList out;
    std::vector<std::string> sigs;
    for (const Term& t : in) {
        std::string sig;  // "signature": which factors with which powers
        for (auto& kv : t.factors) sig += kv.first + "^" + formatNumber(kv.second.power) + ";";
        auto it = std::find(sigs.begin(), sigs.end(), sig);
        if (it == sigs.end()) { sigs.push_back(sig); out.push_back(t); }
        else out[it - sigs.begin()].coef += t.coef;   // 2x + 3x = 5x
    }
    TermList kept;
    for (const Term& t : out)
        if (std::fabs(t.coef) > 1e-12) kept.push_back(t);
    return kept;
}

TermList Simplifier::toTerms(const Ptr& e) {
    switch (e->kind()) {
        case Kind::Num: {
            auto& n = static_cast<const Num&>(*e);
            if (!n.label.empty()) return atom(e, 1);  // keep pi and e as symbols
            if (n.value == 0) return {};
            Term t; t.coef = n.value;
            return {t};
        }
        case Kind::Var: return atom(e, 1);
        case Kind::Neg: {
            TermList t = toTerms(static_cast<const Neg&>(*e).a);
            for (Term& x : t) x.coef = -x.coef;
            return t;
        }
        case Kind::Add: case Kind::Sub: {
            auto& b = static_cast<const Binary&>(*e);
            TermList a = toTerms(b.l), c = toTerms(b.r);
            if (e->kind() == Kind::Sub) for (Term& x : c) x.coef = -x.coef;
            a.insert(a.end(), c.begin(), c.end());
            return combineLikeTerms(a);
        }
        case Kind::Mul: {
            auto& b = static_cast<const Binary&>(*e);
            TermList a = toTerms(b.l), c = toTerms(b.r);
            if (a.size() * c.size() > 12) {           // too big to expand: keep sums as blocks
                if (a.size() > 1) a = atom(fromTerms(a), 1);
                if (c.size() > 1) c = atom(fromTerms(c), 1);
            }
            return multiply(a, c);
        }
        case Kind::Div: {
            auto& b = static_cast<const Binary&>(*e);
            TermList top = toTerms(b.l), bot = toTerms(b.r);
            if (bot.size() == 1) {                     // divide by a single term: invert it
                Term inv;
                inv.coef = 1.0 / bot[0].coef;
                for (auto& kv : bot[0].factors) inv.factors[kv.first] = Factor{kv.second.base, -kv.second.power};
                return multiply(top, TermList{inv});
            }
            if (bot.empty()) return atom(E::div(E::num(1), E::num(0)), 1);
            // Clear fractions inside the denominator: multiply top and bottom
            // by the factors that appear with negative powers.
            //   1/(y/x - x/y)  ->  xy/(y^2 - x^2)
            Term m;
            for (const TermList* list : {&top, &bot})
                for (const Term& t : *list)
                    for (auto& kv : t.factors)
                        if (kv.second.power < 0) {
                            auto it = m.factors.find(kv.first);
                            if (it == m.factors.end()) m.factors[kv.first] = Factor{kv.second.base, -kv.second.power};
                            else it->second.power = std::max(it->second.power, -kv.second.power);
                        }
            m.coef = 1.0 / bot[0].coef;  // also make the first term of the bottom have coefficient 1
            top = multiply(top, TermList{m});
            bot = multiply(bot, TermList{m});
            if (bot.empty()) return atom(E::div(E::num(1), E::num(0)), 1);
            if (bot.size() == 1) return toTerms(E::div(fromTerms(top), fromTerms(bot)));
            return multiply(top, atom(fromTerms(bot), -1));
        }
        case Kind::Pow: {
            auto& b = static_cast<const Binary&>(*e);
            TermList base = toTerms(b.l);
            Ptr ex = simplify(b.r);
            if (E::isNum(ex)) {
                double n = static_cast<const Num&>(*ex).value;
                if (base.size() == 1) {
                    const Term& t = base[0];
                    bool allPowersOne = true;
                    for (auto& kv : t.factors) allPowersOne = allPowersOne && kv.second.power == 1;
                    // (a b)^n = a^n b^n is safe for integer n, or when every power is 1 and coef > 0
                    if (isInteger(n) || (allPowersOne && t.coef > 0)) {
                        Term r; r.coef = std::pow(t.coef, n);
                        for (auto& kv : t.factors) r.factors[kv.first] = Factor{kv.second.base, kv.second.power * n};
                        return {r};
                    }
                }
                if (base.empty()) return {};
                return atom(fromTerms(base), n);  // e.g. (x+1)^2 kept as a block
            }
            return atom(E::pow(fromTerms(base), ex), 1);
        }
        case Kind::Func: {
            auto& f = static_cast<const Func&>(*e);
            Ptr g = E::func(f.name, simplify(f.a));
            if (g->kind() != Kind::Func) return toTerms(g);  // folded to a number
            return atom(g, 1);
        }
    }
    return atom(e, 1);
}

Ptr Simplifier::fromTerm(const Term& t) {
    double p, q;
    asFraction(std::fabs(t.coef), p, q);
    Ptr top = E::num(p), bot = E::num(q);
    for (const Factor& f : sortedFactors(t)) {
        if (f.power > 0) top = E::mul(top, E::pow(f.base, E::num(f.power)));
        else bot = E::mul(bot, E::pow(f.base, E::num(-f.power)));
    }
    Ptr r = E::div(top, bot);
    return t.coef < 0 ? E::neg(r) : r;
}

Ptr Simplifier::fromTerms(const TermList& terms) {
    if (terms.empty()) return E::num(0);
    // Put a positive term first when there is one:  y - x  rather than  -x + y
    TermList order = terms;
    std::stable_partition(order.begin(), order.end(), [](const Term& t) { return t.coef > 0; });
    if (terms[0].coef > 0) order = terms;
    Ptr r = fromTerm(order[0]);
    for (size_t i = 1; i < order.size(); ++i) {
        Term t = order[i];
        if (t.coef < 0) { t.coef = -t.coef; r = E::sub(r, fromTerm(t)); }
        else r = E::add(r, fromTerm(t));
    }
    return r;
}

Ptr Simplifier::simplify(const Ptr& e) { return fromTerms(toTerms(e)); }

}  // namespace ot
