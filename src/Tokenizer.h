// Tokenizer.h - splits the text typed by the user into tokens.
//
// "y = 2cx^2"  ->  [y] [=] [2] [*] [c] [*] [x] [^] [2]
//
// The scanner is a small finite automaton (DFA) with the states
// START, INTEGER, FRACTION and WORD (see Tokenizer.cpp).
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace ot {

enum class TokType { Number, Var, Const, Func, Op, LParen, RParen, Equals };

struct Token {
    TokType type;
    std::string text;   // the characters as typed (or "*" for an inserted multiply)
    double value = 0;   // for Number and Const
    int varIndex = -1;  // for Var: X, Y or C
    int pos = 0;        // 1-based character position, used in error messages
};

// Every input problem is reported with this exception.  The message is
// written for the student, e.g. "Unbalanced brackets: '(' at position 4 ...".
class InputError : public std::runtime_error {
public:
    explicit InputError(const std::string& msg) : std::runtime_error(msg) {}
};

class Tokenizer {
public:
    // paramName: the single letter used for the family parameter (e.g. 'c').
    // Use '\0' when no parameter is allowed (plain number fields).
    explicit Tokenizer(char paramName, bool allowXY = true);

    std::vector<Token> tokenize(const std::string& text) const;

    // Replaces typographic symbols (−, ×, ·, ², ³) by plain ASCII.
    static std::string normalize(const std::string& text);

private:
    char param_;
    bool allowXY_;
    void splitWord(const std::string& word, int pos, std::vector<Token>& out) const;
    static void insertImplicitMultiplication(std::vector<Token>& toks);
};

}  // namespace ot
