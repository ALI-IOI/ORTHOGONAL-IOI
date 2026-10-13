// Parser.h - turns tokens into an expression tree.
//
// Uses Dijkstra's SHUNTING-YARD algorithm with two STACKS:
//   * an operator stack  ( +  -  *  /  ^  unary minus  (  function )
//   * an operand stack   (finished sub-trees)
// Precedence: ^ (highest, right-associative) > unary minus > * / > + -
#pragma once
#include <string>
#include <vector>

#include "Expr.h"
#include "Tokenizer.h"

namespace ot {

struct Equation {
    Ptr lhs, rhs;          // left and right side of "="
    bool hasEquals = false;
    std::string lhsText;   // the left side as typed (trimmed), e.g. "y"
};

class Parser {
public:
    explicit Parser(char paramName, bool allowXY = true) : tok_(paramName, allowXY) {}

    // "y = c x^2"  ->  lhs = y,  rhs = c*x^2
    Equation parseEquation(const std::string& text) const;

    // A single expression without "=" (used for number fields like "-2*pi").
    Ptr parseExpression(const std::string& text) const;

private:
    Tokenizer tok_;
    Ptr build(const std::vector<Token>& toks, size_t from, size_t to) const;
};

}  // namespace ot
