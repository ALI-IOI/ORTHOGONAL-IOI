// Api.cpp - connects all classes:
//   Validator -> Parser -> FamilyAnalyzer -> CurveGenerator / Solver -> JSON
#include "Api.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "CurveGenerator.h"
#include "FamilyAnalyzer.h"
#include "Parser.h"
#include "Solver.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

namespace ot {

namespace {

// ---- tiny JSON writer ----
std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) { char b[8]; std::snprintf(b, 8, "\\u%04x", ch); out += b; }
                else out += ch;
        }
    }
    return out + "\"";
}
std::string jsonNumber(double v) {
    if (!std::isfinite(v)) return "null";
    char b[32];
    std::snprintf(b, sizeof b, "%.6g", v);
    return b;
}
std::string jsonPoints(const Polyline& p) {
    std::string s = "[";
    for (size_t i = 0; i < p.pts.size(); ++i) {
        if (i) s += ',';
        s += jsonNumber(p.pts[i]);
    }
    return s + "]";
}
std::string jsonSteps(const std::vector<Step>& steps) {
    std::string s = "[";
    for (size_t i = 0; i < steps.size(); ++i) {
        if (i) s += ',';
        s += "{\"title\":" + jsonString(steps[i].title) + ",\"math\":" + jsonString(steps[i].math) + "}";
    }
    return s + "]";
}
std::string errorJson(const std::string& msg) {
    return "{\"ok\":false,\"error\":" + jsonString("Invalid information: " + msg) + "}";
}

int pointsInside(const std::vector<Polyline>& curves, const Box& b) {
    int n = 0;
    for (const Polyline& p : curves)
        for (size_t i = 0; i < p.size(); ++i) n += b.contains(p.x(i), p.y(i));
    return n;
}

}  // namespace

std::string runSimulation(const Inputs& in) {
    auto t0 = std::chrono::steady_clock::now();
    try {
        // 1. Check all boxes
        Settings s = Validator::check(in);

        // 2. Parse the equation into a tree
        Equation eq = Parser(s.param).parseEquation(in.equation);

        // 3. Differentiate, eliminate c, build the orthogonal equation
        FamilyAnalyzer fa(eq, s.isExplicit, s.param);
        Ptr rhs = s.isExplicit ? (eq.hasEquals ? eq.rhs : eq.lhs) : nullptr;

        // 4. Curves (computed in an area twice the view so panning shows more)
        Box area = s.view.expanded(0.5);
        CurveGenerator gen(fa, s.isExplicit, rhs, s.view, area);
        std::vector<Polyline> fam = gen.family(s.cValues);
        if (pointsInside(fam, s.view) == 0)
            throw InputError("No curve of the family is visible for these values of " + fa.paramName +
                             " in the chosen x and y range. Try other values (for example x^2 + y^2 = c needs c > 0) "
                             "or a larger range.");
        std::vector<Polyline> traj = gen.trajectories(fam, s.nTraj);
        if (traj.empty())
            throw InputError("No orthogonal trajectory could be drawn: the slope dy/dx is not defined at the tested "
                             "points (∂F/∂x and ∂F/∂y are both 0 or undefined). Check the equation.");

        // 5. Closed form (if possible) and the numerical check
        ClosedForm cf = ClosedFormSolver().solve(fa, s.view);
        std::vector<Step> steps = fa.steps;
        steps.insert(steps.end(), cf.steps.begin(), cf.steps.end());
        steps.push_back({"Graph", "Trajectories traced with RK4, step h = " + jsonNumber(gen.stepSize()) +
                                      " (arc length), starting from points spread over the view."});
        OrthoCheck chk = gen.checkOrthogonality(fam, traj);

        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

        // 6. JSON for the page
        std::string out = "{\"ok\":true";
        out += ",\"closedForm\":" + jsonString(cf.found ? cf.result : "");
        out += ",\"steps\":" + jsonSteps(steps);
        out += ",\"family\":[";
        for (size_t i = 0; i < fam.size(); ++i)
            out += std::string(i ? "," : "") + "{\"c\":" + jsonNumber(fam[i].c) + ",\"pts\":" + jsonPoints(fam[i]) + "}";
        out += "],\"ortho\":[";
        for (size_t i = 0; i < traj.size(); ++i)
            out += std::string(i ? "," : "") + "{\"closed\":" + (traj[i].closed ? "true" : "false") +
                   ",\"pts\":" + jsonPoints(traj[i]) + "}";
        out += "],\"check\":{\"crossings\":" + std::to_string(chk.crossings) +
               ",\"maxAbsCos\":" + jsonNumber(chk.maxAbsCos) + ",\"meanAbsCos\":" + jsonNumber(chk.meanAbsCos) +
               ",\"skipped\":" + std::to_string(chk.skippedSingular) +
               ",\"minAngleDeg\":" + jsonNumber(std::acos(std::min(1.0, chk.maxAbsCos)) * 180 / M_PI) + "}";
        out += ",\"view\":[" + jsonNumber(s.view.xmin) + "," + jsonNumber(s.view.xmax) + "," + jsonNumber(s.view.ymin) +
               "," + jsonNumber(s.view.ymax) + "]";
        out += ",\"view\":[" + jsonNumber(s.view.xmin) + "," + jsonNumber(s.view.xmax) + "," + jsonNumber(s.view.ymin) +
               "," + jsonNumber(s.view.ymax) + "]";
        out += ",\"area\":[" + jsonNumber(area.xmin) + "," + jsonNumber(area.xmax) + "," + jsonNumber(area.ymin) +
               "," + jsonNumber(area.ymax) + "]";
        out += ",\"ms\":" + jsonNumber(ms) + "}";
        return out;
    } catch (const InputError& e) {
        return errorJson(e.what());
    } catch (const std::exception& e) {
        return errorJson(std::string("unexpected problem: ") + e.what());
    }
}

}  // namespace ot

extern "C" EXPORT const char* simulate(const char* type, const char* equation, const char* param, const char* cValues,
                                       const char* nCurves, const char* xmin, const char* xmax, const char* ymin,
                                       const char* ymax, const char* nTraj, double aspect) {
    static std::string result;  // kept alive until the next call so JavaScript can read it
    ot::Inputs in{type, equation, param, cValues, nCurves, xmin, xmax, ymin, ymax, nTraj, aspect};
    result = ot::runSimulation(in);
    return result.c_str();
}
