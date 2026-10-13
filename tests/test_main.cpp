// test_main.cpp - native test program (g++), no browser needed.
// Runs the required examples and invalid inputs and prints the results.
#include <cstdio>
#include <iostream>
#include <regex>
#include <string>

#include "../src/Api.h"

using namespace ot;

static std::string field(const std::string& json, const std::string& key) {
    std::smatch m;
    std::regex re("\"" + key + "\":(\"(?:[^\"\\\\]|\\\\.)*\"|[^,}\\]]*)");
    return std::regex_search(json, m, re) ? m[1].str() : "";
}

static int failures = 0;

static void valid(const char* name, Inputs in, const std::string& expectClosed) {
    std::string j = runSimulation(in);
    std::string ok = field(j, "ok");
    std::cout << "=== " << name << "  [" << in.equation << "]\n";
    if (ok != "true") { std::cout << "  FAIL: " << field(j, "error") << "\n"; ++failures; return; }
    std::regex stepRe("\\{\"title\":\"((?:[^\"\\\\]|\\\\.)*)\",\"math\":\"((?:[^\"\\\\]|\\\\.)*)\"\\}");
    for (std::sregex_iterator it(j.begin(), j.end(), stepRe), end; it != end; ++it)
        std::cout << "  - " << (*it)[1] << ":  " << (*it)[2] << "\n";
    std::string cf = field(j, "closedForm");
    std::cout << "  closed form: " << cf << "\n";
    std::cout << "  crossings=" << field(j, "crossings") << "  max|cos|=" << field(j, "maxAbsCos")
              << "  mean|cos|=" << field(j, "meanAbsCos") << "  skipped(singular)=" << field(j, "skipped")
              << "  time=" << field(j, "ms") << " ms  json=" << j.size() / 1024 << " KB\n";
    bool cfOk = expectClosed.empty() || cf.find(expectClosed) != std::string::npos;
    double maxCos = std::stod(field(j, "maxAbsCos"));
    bool perp = std::stoi(field(j, "crossings")) > 0 && maxCos < 0.05;
    std::cout << "  RESULT: " << (cfOk && perp ? "PASS" : "FAIL") << (cfOk ? "" : " (closed form differs)") << "\n";
    if (!(cfOk && perp)) ++failures;
}

static void invalid(const char* name, Inputs in, const std::string& mustContain) {
    std::string j = runSimulation(in);
    std::string err = field(j, "error");
    bool pass = field(j, "ok") == "false" && err.find(mustContain) != std::string::npos;
    std::cout << "=== INVALID " << name << ": " << err << "\n  RESULT: " << (pass ? "PASS" : "FAIL") << "\n";
    if (!pass) ++failures;
}

static Inputs base(const char* type, const char* eq, const char* cs = "-3 to 3", const char* n = "7") {
    return Inputs{type, eq, "c", cs, n, "-5", "5", "-4", "4", "12"};
}

int main() {
    valid("1. y = c x^2", base("explicit", "y = c x^2"), "x^2 + 2y^2 = k");
    valid("2. x^2 + y^2 = c", base("implicit", "x^2 + y^2 = c", "1 to 16", "6"), "y = kx");
    valid("3. xy = c", base("implicit", "xy = c", "-4, -2, -1, 1, 2, 4"), "x^2 - y^2 = k");
    valid("4. y = c e^x", base("explicit", "y = c e^x", "-2 to 2", "9"), "2x + y^2 = k");
    valid("5. y = cx (lines)", base("explicit", "y = cx"), "x^2 + y^2 = k");
    valid("6a. (x-c)^2 + y^2 = c^2 (c^2 cancels -> linear)", base("implicit", "(x-c)^2 + y^2 = c^2", "-3, -2, -1, 1, 2, 3"), "");
    valid("6b. (x-c)^2 + y^2 = 1 (non-linear in c)", base("implicit", "(x-c)^2 + y^2 = 1", "-3 to 3", "7"), "");
    valid("7. y = c (horizontal lines)", base("explicit", "y = c"), "x = k");
    valid("8. y = sin(x) + c (not separable? ortho -1/cos x)", base("explicit", "y = sin(x) + c"), "");
    valid("9. y^2 = 4cx (parabolas)", base("implicit", "y^2 = 4cx", "-2 to 2", "8"), "2x^2 + y^2 = k");
    valid("10. x = c (vertical lines)", base("implicit", "x = c"), "y = k");

    invalid("empty equation", base("explicit", ""), "is empty");
    invalid("empty c values", base("explicit", "y = cx^2", ""), "Values of c");
    invalid("missing parameter", base("explicit", "y = x^2"), "does not contain the parameter");
    invalid("unbalanced brackets", base("implicit", "(x^2 + y^2 = c"), "Unbalanced brackets");
    invalid("unbalanced brackets 2", base("explicit", "y = c(x+1))"), "Unbalanced brackets");
    invalid("two parameters", base("explicit", "y = a x + c"), "Unknown symbol 'a'");
    invalid("implicit without =", base("implicit", "x^2 + y^2 - c"), "needs an '=' sign");
    invalid("explicit with y on right", base("explicit", "y = c x y"), "must not contain y");
    invalid("parameter cancels", base("explicit", "y = x + c - c"), "cancels out");
    invalid("no x or y", base("implicit", "c = 2"), "neither x nor y");
    invalid("no visible curve", base("implicit", "x^2 + y^2 = c", "-5 to -1", "5"), "No curve of the family is visible");
    invalid("bad range", Inputs{"explicit", "y = cx^2", "c", "1 to 3", "5", "5", "-5", "-4", "4", "12"}, "'x min' must be smaller");
    invalid("bad number", Inputs{"explicit", "y = cx^2", "c", "1 to 3", "5", "abc", "5", "-4", "4", "12"}, "'x min' must be a number");
    invalid("two operators", base("explicit", "y = c*/x"), "two operators in a row");
    invalid("bad parameter name", Inputs{"explicit", "y = cx^2", "x", "1 to 3", "5", "-5", "5", "-4", "4", "12"}, "cannot be called 'x'");

    std::cout << "\n" << (failures ? std::to_string(failures) + " FAILURE(S)" : std::string("ALL TESTS PASSED")) << "\n";
    return failures ? 1 : 0;
}
