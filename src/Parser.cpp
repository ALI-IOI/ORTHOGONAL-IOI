// Parser.cpp - shunting-yard algorithm that builds the expression tree.
#include "Parser.h"

#include <stack>

namespace ot {

namespace {

// One entry of the operator stack.
struct OpEntry {
    char op;           // + - * / ^, 'n' = unary minus, '(' = bracket, 'f' = function
    int pos;           // position in the text (for error messages)
    char bracket;      // which bracket was opened: ( [ {
    std::string func;  // function name when op == 'f'
};

int precedence(char op) {
    switch (op) {
        case '+': case '-': return 1;
        case '*': case '/': return 2;
        case 'n': return 3;  // unary minus
        case '^': return 4;
        default: return 0;
    }
}
bool rightAssociative(char op) { return op == '^' || op == 'n'; }

char closing(char open) { return open == '(' ? ')' : open == '[' ? ']' : '}'; }

std::string at(int pos) { return " at position " + std::to_string(pos); }

// Pops one operator and combines the operands on top of the operand stack.
void applyOp(const OpEntry& e, std::stack<Ptr>& vals) {
    if (e.op == 'n') {
        if (vals.empty()) throw InputError("The minus sign" + at(e.pos) + " has nothing after it.");
        Ptr a = vals.top(); vals.pop();
        vals.push(E::neg(a));
        return;
    }
    if (vals.size() < 2)
        throw InputError(std::string("The operator '") + e.op + "'" + at(e.pos) + " is missing a number or variable.");
    Ptr b = vals.top(); vals.pop();
    Ptr a = vals.top(); vals.pop();
    switch (e.op) {
        case '+': vals.push(E::add(a, b)); break;
        case '-': vals.push(E::sub(a, b)); break;
        case '*': vals.push(E::mul(a, b)); break;
        case '/': vals.push(E::div(a, b)); break;
        case '^': vals.push(E::pow(a, b)); break;
    }
}

}  // namespace

Ptr Parser::build(const std::vector<Token>& toks, size_t from, size_t to) const {
    std::stack<OpEntry> ops;
    std::stack<Ptr> vals;
    bool expectOperand = true;  // true when the next token must start a value

    for (size_t i = from; i < to; ++i) {
        const Token& t = toks[i];
        switch (t.type) {
            case TokType::Number:
                vals.push(E::num(t.value));
                expectOperand = false;
                break;
            case TokType::Const:
                vals.push(std::make_shared<Num>(t.value, t.text));
                expectOperand = false;
                break;
            case TokType::Var:
                vals.push(E::var(t.varIndex, t.text));
                expectOperand = false;
                break;
            case TokType::Func:
                if (i + 1 >= to || toks[i + 1].type != TokType::LParen)
                    throw InputError("The function '" + t.text + "'" + at(t.pos) +
                                     " must be followed by a bracket, for example " + t.text + "(x).");
                ops.push(OpEntry{'f', t.pos, 0, t.text});
                break;
            case TokType::LParen:
                ops.push(OpEntry{'(', t.pos, t.text[0], ""});
                expectOperand = true;
                break;
            case TokType::RParen: {
                if (expectOperand) {
                    if (i > from && toks[i - 1].type == TokType::LParen)
                        throw InputError("Empty brackets '" + toks[i - 1].text + t.text + "'" + at(toks[i - 1].pos) + ".");
                    throw InputError("Something is missing before '" + t.text + "'" + at(t.pos) + ".");
                }
                while (!ops.empty() && ops.top().op != '(') { applyOp(ops.top(), vals); ops.pop(); }
                if (ops.empty())
                    throw InputError("Unbalanced brackets: '" + t.text + "'" + at(t.pos) + " has no matching opening bracket.");
                if (closing(ops.top().bracket) != t.text[0])
                    throw InputError(std::string("Unbalanced brackets: '") + ops.top().bracket + "'" + at(ops.top().pos) +
                                     " is closed by '" + t.text + "'" + at(t.pos) + ".");
                ops.pop();  // remove "("
                if (!ops.empty() && ops.top().op == 'f') {  // bracket belonged to a function
                    Ptr a = vals.top(); vals.pop();
                    vals.push(E::func(ops.top().func, a));
                    ops.pop();
                }
                expectOperand = false;
                break;
            }
            case TokType::Op: {
                char op = t.text[0];
                if (expectOperand) {  // a sign, not a binary operator
                    if (op == '+') break;  // unary plus does nothing
                    if (op == '-') { ops.push(OpEntry{'n', t.pos, 0, ""}); break; }
                    throw InputError(std::string("The operator '") + op + "'" + at(t.pos) +
                                     " has nothing on its left (two operators in a row?).");
                }
                // Pop operators that must be done first.
                while (!ops.empty() && ops.top().op != '(' && ops.top().op != 'f') {
                    int pt = precedence(ops.top().op), pc = precedence(op);
                    if (pt > pc || (pt == pc && !rightAssociative(op))) { applyOp(ops.top(), vals); ops.pop(); }
                    else break;
                }
                ops.push(OpEntry{op, t.pos, 0, ""});
                expectOperand = true;
                break;
            }
            case TokType::Equals:
                throw InputError("Unexpected '='" + at(t.pos) + ".");
        }
    }
    if (expectOperand) {
        if (from == to) throw InputError("The expression is empty.");
        throw InputError("The expression ends with an operator; something is missing at the end.");
    }
    while (!ops.empty()) {
        if (ops.top().op == '(')
            throw InputError(std::string("Unbalanced brackets: '") + ops.top().bracket + "'" + at(ops.top().pos) +
                             " is never closed.");
        applyOp(ops.top(), vals);
        ops.pop();
    }
    return vals.top();
}

Ptr Parser::parseExpression(const std::string& text) const {
    std::vector<Token> toks = tok_.tokenize(text);
    for (const Token& t : toks)
        if (t.type == TokType::Equals) throw InputError("'=' is not allowed here" + at(t.pos) + ".");
    return build(toks, 0, toks.size());
}

Equation Parser::parseEquation(const std::string& text) const {
    std::vector<Token> toks = tok_.tokenize(text);
    Equation eq;
    size_t eqAt = toks.size();
    for (size_t i = 0; i < toks.size(); ++i) {
        if (toks[i].type != TokType::Equals) continue;
        if (eq.hasEquals) throw InputError("The equation has more than one '=' sign (second one" + at(toks[i].pos) + ").");
        eq.hasEquals = true;
        eqAt = i;
    }
    if (!eq.hasEquals) {
        eq.lhs = build(toks, 0, toks.size());
        return eq;
    }
    if (eqAt == 0) throw InputError("There is nothing on the left side of '='.");
    if (eqAt + 1 == toks.size()) throw InputError("There is nothing on the right side of '='.");
    eq.lhs = build(toks, 0, eqAt);
    eq.rhs = build(toks, eqAt + 1, toks.size());
    std::string left = Tokenizer::normalize(text).substr(0, toks[eqAt].pos - 1);
    size_t a = left.find_first_not_of(" \t"), b = left.find_last_not_of(" \t");
    eq.lhsText = (a == std::string::npos) ? "" : left.substr(a, b - a + 1);
    return eq;
}

}  // namespace ot
