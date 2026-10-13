// Api.h - the single entry point used by the web page.
#pragma once
#include <string>

#include "Validator.h"

namespace ot {

// Runs the whole pipeline and returns the result as JSON text:
//   { "ok": true, "steps": [...], "family": [...], "ortho": [...], "check": {...} }
// or
//   { "ok": false, "error": "Invalid information: ..." }
std::string runSimulation(const Inputs& in);

}  // namespace ot

// Called from JavaScript (all arguments are the raw text of the input boxes).
extern "C" const char* simulate(const char* type, const char* equation, const char* param, const char* cValues,
                                const char* nCurves, const char* xmin, const char* xmax, const char* ymin,
                                const char* ymax, const char* nTraj, double aspect);
