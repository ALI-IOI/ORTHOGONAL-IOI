// Types.h - small data types shared by several files.
#pragma once
#include <cmath>
#include <string>
#include <vector>

namespace ot {

// A rectangle of the plane: the visible graph area or the area we compute in.
struct Box {
    double xmin, xmax, ymin, ymax;
    double width() const { return xmax - xmin; }
    double height() const { return ymax - ymin; }
    bool contains(double x, double y) const { return x >= xmin && x <= xmax && y >= ymin && y <= ymax; }
    Box expanded(double factor) const {  // grow by factor*size on every side
        double dx = width() * factor, dy = height() * factor;
        return Box{xmin - dx, xmax + dx, ymin - dy, ymax + dy};
    }
};

// One curve to draw: points stored as x0, y0, x1, y1, ...
struct Polyline {
    std::vector<double> pts;
    double c = NAN;      // parameter value (family curves only)
    bool closed = false; // true when the curve returns to its start (e.g. an ellipse)
    std::vector<double> cs;  // trajectories of non-linear families: value of c at each point
    size_t size() const { return pts.size() / 2; }
    double x(size_t i) const { return pts[2 * i]; }
    double y(size_t i) const { return pts[2 * i + 1]; }
    void add(double px, double py) { pts.push_back(px); pts.push_back(py); }
};

// One line of the derivation shown under the graph.
struct Step {
    std::string title;
    std::string math;
};

// Halton low-discrepancy sequence: evenly spread "random-looking" numbers in [0,1).
inline double halton(int index, int base) {
    double f = 1, r = 0;
    for (int i = index; i > 0; i /= base) { f /= base; r += f * (i % base); }
    return r;
}

}  // namespace ot
