// Tokenizer.cpp - the scanner (lexical analysis).
#include "Tokenizer.h"

#include <cctype>
#include <cmath>

#include "Expr.h"

namespace ot {

Tokenizer::Tokenizer(char paramName, bool allowXY) : param_(paramName), allowXY_(allowXY) {}

std::string Tokenizer::normalize(const std::string& s) {
    // UTF-8 byte sequences of symbols often copied from PDFs or Word.
    struct Rep { const char* from; const char* to; };
    static const Rep reps[] = {{"\xE2\x88\x92", "-"},   // − minus sign
                               {"\xC3\x97", "*"},       // × times
                               {"\xC2\xB7", "*"},       // · middle dot
                               {"\xE2\x8B\x85", "*"},   // ⋅ dot operator
                               {"\xC2\xB2", "^2"},      // ²
                               {"\xC2\xB3", "^3"},      // ³
                               {"\xCF\x80", "pi"}};     // π
    std::string out = s;
    for (const Rep& r : reps) {
        std::string from = r.from;
        size_t at;
        while ((at = out.find(from)) != std::string::npos) out.replace(at, from.size(), r.to);
    }
    return out;
}

// ----------------------------------------------------------------------
// The DFA.  One character is read at a time; the current state decides
// what the character means.
//   START    : between tokens
//   INTEGER  : inside the digits of a number   (12)
//   FRACTION : after the decimal point          (12.5)
//   WORD     : inside a run of letters          (sin, cx, pi)
// ----------------------------------------------------------------------
std::vector<Token> Tokenizer::tokenize(const std::string& raw) const {
    const std::string s = normalize(raw);
    enum State { START, INTEGER, FRACTION, WORD };
    State state = START;
    std::vector<Token> toks;
    std::string buf;
    int bufStart = 0;

    auto flush = [&]() {  // finish the token stored in buf
        if (state == INTEGER || state == FRACTION) {
            if (buf == ".") throw InputError("A lone '.' at position " + std::to_string(bufStart) + " is not a number.");
            Token t{TokType::Number, buf, std::stod(buf), -1, bufStart};
            toks.push_back(t);
        } else if (state == WORD) {
            splitWord(buf, bufStart, toks);
        }
        buf.clear();
        state = START;
    };

    for (size_t i = 0; i <= s.size(); ++i) {
        char ch = (i < s.size()) ? s[i] : '\0';  // '\0' marks the end of input
        int pos = static_cast<int>(i) + 1;
        bool digit = std::isdigit(static_cast<unsigned char>(ch));
        bool letter = std::isalpha(static_cast<unsigned char>(ch));

        // Transitions that continue the current token
        if (state == INTEGER && digit) { buf += ch; continue; }
        if (state == INTEGER && ch == '.') { buf += ch; state = FRACTION; continue; }
        if (state == FRACTION && digit) { buf += ch; continue; }
        if (state == FRACTION && ch == '.')
            throw InputError("The number at position " + std::to_string(bufStart) + " has two decimal points.");
        if (state == WORD && letter) { buf += ch; continue; }

        flush();  // any other character ends the current token
        if (ch == '\0') break;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue;
        if (digit || ch == '.') { state = (ch == '.') ? FRACTION : INTEGER; buf = ch; bufStart = pos; continue; }
        if (letter) { state = WORD; buf = ch; bufStart = pos; continue; }

        Token t{TokType::Op, std::string(1, ch), 0, -1, pos};
        switch (ch) {
            case '+': case '-': case '*': case '/': case '^': break;
            case '(': case '[': case '{': t.type = TokType::LParen; break;
            case ')': case ']': case '}': t.type = TokType::RParen; break;
            case '=': t.type = TokType::Equals; break;
            default:
                throw InputError(std::string("The character '") + ch + "' at position " + std::to_string(pos) +
                                 " is not allowed. Use numbers, letters, + - * / ^ ( ) and =.");
        }
        toks.push_back(t);
    }
    insertImplicitMultiplication(toks);
    return toks;
}

// A run of letters such as "cx", "sinx" or "pi" is split into known names,
// always taking the LONGEST name that matches (so "exp" is the function exp,
// not e*x*p).
void Tokenizer::splitWord(const std::string& word, int pos, std::vector<Token>& out) const {
    size_t i = 0;
    while (i < word.size()) {
        int p = pos + static_cast<int>(i);
        bool matched = false;
        for (const std::string& f : knownFunctions()) {           // functions first (longest)
            if (word.compare(i, f.size(), f) == 0) {
                out.push_back(Token{TokType::Func, f, 0, -1, p});
                i += f.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;
        if (word.compare(i, 2, "pi") == 0) {
            out.push_back(Token{TokType::Const, "pi", M_PI, -1, p});
            i += 2;
            continue;
        }
        char ch = word[i];
        Token t{TokType::Var, std::string(1, ch), 0, -1, p};
        if (allowXY_ && ch == 'x') t.varIndex = X;
        else if (allowXY_ && ch == 'y') t.varIndex = Y;
        else if (param_ != '\0' && ch == param_) t.varIndex = C;
        else if (ch == 'e') { t.type = TokType::Const; t.value = M_E; }
        else {
            std::string allowed = allowXY_ ? "x, y" : "";
            if (param_ != '\0') allowed += std::string(", the parameter ") + param_;
            if (allowed.empty()) allowed = "numbers";
            throw InputError(std::string("Unknown symbol '") + ch + "' at position " + std::to_string(p) +
                             ". Only " + allowed + ", pi, e and functions such as sin, cos, exp, ln, sqrt are allowed." +
                             (allowXY_ ? " A family must have exactly one parameter." : ""));
        }
        out.push_back(t);
        ++i;
    }
}

// "2x" means 2*x, "x(y+1)" means x*(y+1), "(x+1)(x-1)" means (x+1)*(x-1).
void Tokenizer::insertImplicitMultiplication(std::vector<Token>& toks) {
    std::vector<Token> out;
    for (size_t i = 0; i < toks.size(); ++i) {
        if (i > 0) {
            TokType a = toks[i - 1].type, b = toks[i].type;
            bool leftEndsValue = a == TokType::Number || a == TokType::Var || a == TokType::Const || a == TokType::RParen;
            bool rightStartsValue = b == TokType::Number || b == TokType::Var || b == TokType::Const ||
                                    b == TokType::Func || b == TokType::LParen;
            if (leftEndsValue && rightStartsValue)
                out.push_back(Token{TokType::Op, "*", 0, -1, toks[i].pos});
        }
        out.push_back(toks[i]);
    }
    toks.swap(out);
}

}  // namespace ot
