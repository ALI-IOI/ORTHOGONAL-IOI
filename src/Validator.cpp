// Validator.cpp
#include "Validator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "Parser.h"

namespace ot {

namespace {
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string lower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}
}  // namespace

void Validator::requireFilled(const std::string& value, const std::string& label) {
    if (trim(value).empty()) throw InputError("The field '" + label + "' is empty. Please fill it in.");
}

// A number field may hold a constant expression such as -2*pi or 1/2.
double Validator::number(const std::string& text, const std::string& label) {
    requireFilled(text, label);
    double v;
    try {
        v = Parser('\0', false).parseExpression(text)->eval(Env());
    } catch (const InputError& e) {
        throw InputError("'" + label + "' must be a number (you typed '" + trim(text) + "'). " + e.what());
    }
    if (!std::isfinite(v)) throw InputError("'" + label + "' is not a finite number.");
    return v;
}

int Validator::wholeNumber(const std::string& text, const std::string& label, int lo, int hi) {
    double v = number(text, label);
    if (v != std::round(v) || v < lo || v > hi)
        throw InputError("'" + label + "' must be a whole number from " + std::to_string(lo) + " to " +
                         std::to_string(hi) + ".");
    return static_cast<int>(v);
}

// "-3 to 3" (with a number of curves), "-3 : 3", or a list "1, 2, 5".
std::vector<double> Validator::parameterValues(const std::string& text, const std::string& countText, char param) {
    const std::string label = std::string("Values of ") + param;
    requireFilled(text, label);
    std::string t = lower(text);
    size_t at = std::string::npos, len = 0;
    for (const char* sep : {" to ", "..", ":"}) {
        size_t p = t.find(sep);
        if (p != std::string::npos) { at = p; len = std::string(sep).size(); break; }
    }
    std::vector<double> out;
    if (at != std::string::npos) {  // a range
        double a = number(text.substr(0, at), label + " (start of range)");
        double b = number(text.substr(at + len), label + " (end of range)");
        if (a == b) throw InputError("The range of " + std::string(1, param) + " starts and ends at the same value.");
        int n = wholeNumber(countText, "Number of curves", 1, 40);
        for (int i = 0; i < n; ++i) out.push_back(n == 1 ? a : a + (b - a) * i / (n - 1));
    } else {                        // a list
        size_t start = 0;
        while (start <= text.size()) {
            size_t comma = text.find_first_of(",;", start);
            std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            out.push_back(number(item, label + " (item " + std::to_string(out.size() + 1) + ")"));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        if (out.size() > 40) throw InputError("Please give at most 40 values of " + std::string(1, param) + ".");
    }
    return out;
}

Settings Validator::check(const Inputs& in) {
    Settings s;
    // (a) required fields
    requireFilled(in.type, "Type of equation");
    requireFilled(in.equation, "Equation of the family");
    requireFilled(in.param, "Parameter name");

    if (in.type == "explicit") s.isExplicit = true;
    else if (in.type == "implicit") s.isExplicit = false;
    else throw InputError("'Type of equation' must be explicit or implicit.");

    std::string p = trim(in.param);
    if (p.size() != 1 || !std::isalpha(static_cast<unsigned char>(p[0])))
        throw InputError("The parameter name must be a single letter such as c or k (you typed '" + p + "').");
    if (p == "x" || p == "y" || p == "e")
        throw InputError("The parameter cannot be called '" + p + "' because that letter already means something. Use c or k.");
    s.param = p[0];

    s.cValues = parameterValues(in.cValues, in.nCurves, s.param);
    s.view.xmin = number(in.xmin, "x min");
    s.view.xmax = number(in.xmax, "x max");
    s.view.ymin = number(in.ymin, "y min");
    s.view.ymax = number(in.ymax, "y max");
    if (s.view.xmin >= s.view.xmax) throw InputError("'x min' must be smaller than 'x max'.");
    if (s.view.ymin >= s.view.ymax) throw InputError("'y min' must be smaller than 'y max'.");
    if (s.view.width() > 1e6 || s.view.height() > 1e6 || s.view.width() < 1e-6 || s.view.height() < 1e-6)
        throw InputError("The view range is too large or too small.");
    s.nTraj = wholeNumber(in.nTraj, "Number of orthogonal curves", 1, 60);

    // Equal scale on both axes (like Desmos), so that right angles LOOK right:
    // widen the shorter side of the range until it matches the screen shape.
    s.typedView = s.view;
    if (in.aspect > 0 && std::isfinite(in.aspect)) {
        double cx = (s.view.xmin + s.view.xmax) / 2, cy = (s.view.ymin + s.view.ymax) / 2;
        double w = s.view.width(), h = s.view.height();
        if (w / h < in.aspect) w = h * in.aspect;
        else h = w / in.aspect;
        s.view = Box{cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2};
    }
    return s;
}

}  // namespace ot
