// Validator.h - checks every input box before any drawing is done.
// Each problem throws InputError with a message that says what to fix.
#pragma once
#include <string>
#include <vector>

#include "Types.h"

namespace ot {

// The raw text of every input box, exactly as typed.
struct Inputs {
    std::string type;       // "explicit" or "implicit"
    std::string equation;
    std::string param;      // name of the parameter, e.g. "c"
    std::string cValues;    // "-3 to 3"  or  "1, 2, 4"
    std::string nCurves;    // used with a range
    std::string xmin, xmax, ymin, ymax;
    std::string nTraj;      // number of orthogonal curves
    double aspect = 0;      // width/height of the drawing area in pixels (0 = keep the range as typed)
};

// The same information after checking, converted to numbers.
struct Settings {
    bool isExplicit = true;
    char param = 'c';
    std::vector<double> cValues;
    Box view{-5, 5, -5, 5};
    int nTraj = 12;
    Box typedView{-5, 5, -5, 5};  // the range exactly as typed (before fitting to the screen)
};

class Validator {
public:
    static Settings check(const Inputs& in);

private:
    static void requireFilled(const std::string& value, const std::string& label);
    static double number(const std::string& text, const std::string& label);
    static int wholeNumber(const std::string& text, const std::string& label, int lo, int hi);
    static std::vector<double> parameterValues(const std::string& text, const std::string& countText, char param);
};

}  // namespace ot
