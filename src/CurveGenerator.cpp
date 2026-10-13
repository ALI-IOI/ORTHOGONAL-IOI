// CurveGenerator.cpp
#include "CurveGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#ifdef OT_DEBUG
#include <cstdio>
#endif

namespace ot {

CurveGenerator::CurveGenerator(const FamilyAnalyzer& fa, bool isExplicit, Ptr explicitRhs, const Box& view, const Box& area)
    : fa_(fa), isExplicit_(isExplicit), rhs_(std::move(explicitRhs)), view_(view), area_(area) {
    h_ = std::min(view.width(), view.height()) / 250.0;
}

// ----------------------------------------------------------------------
//  Family curves
// ----------------------------------------------------------------------
std::vector<Polyline> CurveGenerator::family(const std::vector<double>& cValues) const {
    std::vector<Polyline> out;
    for (double c : cValues) {
        if (isExplicit_) explicitCurve(c, out);
        else marchingSquares(c, out);
    }
    return out;
}

// y = f(x, c): walk along x; start a new piece where the curve leaves the
// area, is undefined (e.g. ln of a negative number) or jumps (asymptote).
void CurveGenerator::explicitCurve(double c, std::vector<Polyline>& out) const {
    const int N = 1000;
    Box ext = area_.expanded(0.5);
    Polyline cur;
    cur.c = c;
    double prevY = NAN;
    auto finish = [&]() {
        if (cur.size() >= 2) out.push_back(cur);
        cur.pts.clear();
    };
    for (int i = 0; i <= N; ++i) {
        double x = area_.xmin + area_.width() * i / N;
        double y = rhs_->eval(Env(x, 0, c));
        bool ok = std::isfinite(y) && y > ext.ymin && y < ext.ymax;
        if (ok && cur.size() > 0 && std::fabs(y - prevY) > area_.height()) finish();  // jump
        if (ok) cur.add(x, y);
        else finish();
        prevY = y;
    }
    finish();
}

// MARCHING SQUARES: evaluate F on a grid; wherever F changes sign along a
// grid edge the curve F = 0 crosses that edge (position by linear
// interpolation). Inside each cell the crossings are joined by a segment,
// and the segments are then chained into long polylines.
void CurveGenerator::marchingSquares(double c, std::vector<Polyline>& out) const {
    const int N = 180;
    const double dx = area_.width() / N, dy = area_.height() / N;
    // The grid is shifted by an "odd" fraction of a cell so that grid points
    // do not fall exactly on nice values such as x = 0 (where F may be exactly 0).
    const double x0 = area_.xmin - 0.3719 * dx, y0 = area_.ymin - 0.2853 * dy;
    std::vector<double> v((N + 1) * (N + 1));
    for (int j = 0; j <= N; ++j)
        for (int i = 0; i <= N; ++i) v[j * (N + 1) + i] = fa_.evalF(x0 + i * dx, y0 + j * dy, c);
    auto val = [&](int i, int j) { return v[j * (N + 1) + i]; };

    // Edge numbering: horizontal edges first, then vertical edges.
    const int HCOUNT = N * (N + 1), TOTAL = HCOUNT + (N + 1) * N;
    auto hId = [&](int i, int j) { return j * N + i; };              // (i,j)-(i+1,j)
    auto vId = [&](int i, int j) { return HCOUNT + j * (N + 1) + i; };  // (i,j)-(i,j+1)
    std::vector<double> ex(TOTAL, NAN), ey(TOTAL, NAN);

    auto crossing = [&](int id, double xa, double ya, double va, double xb, double yb, double vb) {
        if (!std::isfinite(va) || !std::isfinite(vb) || (va > 0) == (vb > 0)) return;
        double t = va / (va - vb);
        double px = xa + t * (xb - xa), py = ya + t * (yb - ya);
        double fp = fa_.evalF(px, py, c);
        // A sign change across a pole (e.g. 1/x) is not a real crossing: F stays large there.
        if (!std::isfinite(fp) || std::fabs(fp) > 0.5 * (std::fabs(va) + std::fabs(vb))) return;
        ex[id] = px; ey[id] = py;
    };
    for (int j = 0; j <= N; ++j)
        for (int i = 0; i <= N; ++i) {
            double x = x0 + i * dx, y = y0 + j * dy;
            if (i < N) crossing(hId(i, j), x, y, val(i, j), x + dx, y, val(i + 1, j));
            if (j < N) crossing(vId(i, j), x, y, val(i, j), x, y + dy, val(i, j + 1));
        }

    // Segments inside each cell
    std::vector<std::array<int, 2>> segs;
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            int bottom = hId(i, j), top = hId(i, j + 1), left = vId(i, j), right = vId(i + 1, j);
            std::vector<int> e;
            for (int id : {bottom, right, top, left})
                if (!std::isnan(ex[id])) e.push_back(id);
            if (e.size() == 2) segs.push_back({e[0], e[1]});
            else if (e.size() == 4) {  // saddle cell: use the centre value to decide
                double centre = fa_.evalF(x0 + (i + 0.5) * dx, y0 + (j + 0.5) * dy, c);
                if ((centre > 0) == (val(i, j) > 0)) { segs.push_back({bottom, right}); segs.push_back({top, left}); }
                else { segs.push_back({bottom, left}); segs.push_back({right, top}); }
            }
        }

    // Chain segments that share an edge into polylines.
    std::vector<std::array<int, 2>> adj(TOTAL, {-1, -1});
    for (int s = 0; s < static_cast<int>(segs.size()); ++s)
        for (int id : segs[s]) (adj[id][0] < 0 ? adj[id][0] : adj[id][1]) = s;
    std::vector<bool> used(segs.size(), false);
    auto walk = [&](int seg, int edge, std::vector<int>& chain) {  // follow from 'edge' away from 'seg'
        while (true) {
            int next = adj[edge][0] == seg ? adj[edge][1] : adj[edge][0];
            if (next < 0 || used[next]) return;
            used[next] = true;
            edge = segs[next][0] == edge ? segs[next][1] : segs[next][0];
            chain.push_back(edge);
            seg = next;
        }
    };
    for (int s = 0; s < static_cast<int>(segs.size()); ++s) {
        if (used[s]) continue;
        used[s] = true;
        std::vector<int> fwd{segs[s][1]}, back;
        walk(s, segs[s][1], fwd);
        walk(s, segs[s][0], back);
        Polyline p;
        p.c = c;
        for (auto it = back.rbegin(); it != back.rend(); ++it) p.add(ex[*it], ey[*it]);
        p.add(ex[segs[s][0]], ey[segs[s][0]]);
        for (int id : fwd) p.add(ex[id], ey[id]);
        if (p.size() >= 3) out.push_back(p);
    }
}

// ----------------------------------------------------------------------
//  Orthogonal trajectories
// ----------------------------------------------------------------------
std::vector<Polyline> CurveGenerator::trajectories(const std::vector<Polyline>& fam, int count) const {
    struct Seed { double x, y, c; };
    std::vector<Seed> seeds;

    // 1) seeds on the family curves (every trajectory must cross them),
    //    taken in a spread-out order and alternating between curves
    std::vector<std::vector<Seed>> perCurve;
    for (const Polyline& p : fam) {
        std::vector<Seed> s;
        for (size_t i = 0; i < p.size(); i += 3)
            if (view_.contains(p.x(i), p.y(i))) s.push_back({p.x(i), p.y(i), p.c});
        // reorder: middle first, then quarters, then eighths, ...
        std::vector<Seed> spread;
        for (int k = 1; k <= static_cast<int>(s.size()); ++k) {
            size_t idx = static_cast<size_t>(halton(k, 2) * s.size());
            spread.push_back(s[idx]);
        }
        perCurve.push_back(spread);
    }
    for (size_t k = 0;; ++k) {
        bool any = false;
        for (auto& v : perCurve)
            if (k < v.size()) { seeds.push_back(v[k]); any = true; }
        if (!any) break;
    }
    // 2) evenly spread extra points (only when c has a formula c = c(x, y))
    if (fa_.linearInC)
        for (int k = 1; k <= 400; ++k)
            seeds.push_back({view_.xmin + halton(k, 2) * view_.width(), view_.ymin + halton(k, 3) * view_.height(), 0});

    // Keep trajectories apart: a seed closer than 'sep' to an existing
    // trajectory is skipped.  A grid of buckets makes this check fast.
    const double sep = std::sqrt(view_.width() * view_.height() / std::max(count, 1)) * 0.42;
    std::unordered_map<long long, std::vector<std::pair<double, double>>> grid;
    auto key = [&](long long gx, long long gy) { return gx * 1000003LL + gy; };
    auto cell = [&](double v, double lo) { return static_cast<long long>(std::floor((v - lo) / sep)); };
    auto tooClose = [&](double x, double y) {
        long long gx = cell(x, area_.xmin), gy = cell(y, area_.ymin);
        for (long long a = gx - 1; a <= gx + 1; ++a)
            for (long long b = gy - 1; b <= gy + 1; ++b) {
                auto it = grid.find(key(a, b));
                if (it == grid.end()) continue;
                for (auto& q : it->second)
                    if (std::hypot(q.first - x, q.second - y) < sep) return true;
            }
        return false;
    };

    RK4Tracer tracer(fa_, area_, h_);
    std::vector<Polyline> out;
    for (const Seed& s : seeds) {
        if (static_cast<int>(out.size()) >= count) break;
        if (tooClose(s.x, s.y)) continue;
        Polyline p = tracer.trace(s.x, s.y, s.c);
        if (p.size() < 5) continue;
        for (size_t i = 0; i < p.size(); ++i)
            grid[key(cell(p.x(i), area_.xmin), cell(p.y(i), area_.ymin))].push_back({p.x(i), p.y(i)});
        out.push_back(std::move(p));
    }
    return out;
}

// ----------------------------------------------------------------------
//  Numerical check: tangents perpendicular at every crossing?
// ----------------------------------------------------------------------
OrthoCheck CurveGenerator::checkOrthogonality(const std::vector<Polyline>& fam, const std::vector<Polyline>& traj) const {
    // Tangent of a polyline at segment i: direction from point i-1 to point i+2
    // (a central difference, more accurate than the segment alone).
    // 'bend' = angle between the neighbouring segments; a large bend means the
    // polyline is too coarse there to give a reliable tangent.
    auto tangent = [](const Polyline& p, size_t i, double& tx, double& ty, double& bend) {
        size_t a = i > 0 ? i - 1 : i, b = std::min(i + 2, p.size() - 1);
        tx = p.x(b) - p.x(a); ty = p.y(b) - p.y(a);
        double ux = p.x(i + 1) - p.x(a), uy = p.y(i + 1) - p.y(a), vx = p.x(b) - p.x(i), vy = p.y(b) - p.y(i);
        double nu = std::hypot(ux, uy), nv = std::hypot(vx, vy);
        bend = (nu > 0 && nv > 0) ? std::acos(std::clamp((ux * vx + uy * vy) / (nu * nv), -1.0, 1.0)) : 0;
    };
    struct Seg { int curve; size_t i; };
    std::vector<Seg> famSegs;
    for (int k = 0; k < static_cast<int>(fam.size()); ++k) {
        const Polyline& p = fam[k];
        for (size_t i = 0; i + 1 < p.size(); ++i)
            if (view_.contains(p.x(i), p.y(i)) && view_.contains(p.x(i + 1), p.y(i + 1)) &&
                std::hypot(p.x(i + 1) - p.x(i), p.y(i + 1) - p.y(i)) > 1e-12)
                famSegs.push_back({k, i});
    }

    // Buckets of family segments (64 x 64 grid over the view)
    const int G = 64;
    auto gxOf = [&](double x) { return std::clamp(static_cast<int>((x - view_.xmin) / view_.width() * G), 0, G - 1); };
    auto gyOf = [&](double y) { return std::clamp(static_cast<int>((y - view_.ymin) / view_.height() * G), 0, G - 1); };
    std::vector<std::vector<int>> bucket(G * G);
    for (int s = 0; s < static_cast<int>(famSegs.size()); ++s) {
        const Polyline& p = fam[famSegs[s].curve];
        size_t i = famSegs[s].i;
        for (int a = gxOf(std::min(p.x(i), p.x(i + 1))); a <= gxOf(std::max(p.x(i), p.x(i + 1))); ++a)
            for (int b = gyOf(std::min(p.y(i), p.y(i + 1))); b <= gyOf(std::max(p.y(i), p.y(i + 1))); ++b)
                bucket[b * G + a].push_back(s);
    }

    struct Hit { double cosv, grad, fc; bool coarse; };
    std::vector<Hit> hits;
    std::vector<int> stamp(famSegs.size(), -1);
    int trajSegId = 0;
    for (const Polyline& p : traj)
        for (size_t i = 0; i + 1 < p.size(); ++i, ++trajSegId) {
            double ax = p.x(i), ay = p.y(i), bx = p.x(i + 1), by = p.y(i + 1);
            if (!view_.contains(ax, ay) || !view_.contains(bx, by)) continue;
            for (int a = gxOf(std::min(ax, bx)); a <= gxOf(std::max(ax, bx)); ++a)
                for (int b = gyOf(std::min(ay, by)); b <= gyOf(std::max(ay, by)); ++b)
                    for (int s : bucket[b * G + a]) {
                        if (stamp[s] == trajSegId) continue;  // already tested this pair
                        stamp[s] = trajSegId;
                        const Polyline& f = fam[famSegs[s].curve];
                        size_t j = famSegs[s].i;
                        // Solve A + t (B - A) = F1 + u (F2 - F1) for the crossing point
                        double rx = bx - ax, ry = by - ay, sx = f.x(j + 1) - f.x(j), sy = f.y(j + 1) - f.y(j);
                        double den = rx * sy - ry * sx;
                        if (std::fabs(den) < 1e-300) continue;
                        double t = ((f.x(j) - ax) * sy - (f.y(j) - ay) * sx) / den;
                        double u = ((f.x(j) - ax) * ry - (f.y(j) - ay) * rx) / den;
                        if (t < 0 || t >= 1 || u < 0 || u >= 1) continue;
                        // Trajectory tangent: measured from the RK4 points (numerical).
                        // Family tangent: exact, from the family's own equation with its own c,
                        //   tangent = (Fy, -Fx)  because  Fx dx + Fy dy = 0 along the curve.
                        double tx1, ty1, b1, b2 = 0, tx2, ty2;
                        tangent(p, i, tx1, ty1, b1);
                        double px = ax + t * rx, py = ay + t * ry;
                        if (!p.cs.empty()) {
                            // Non-linear family: several members may pass through one point.
                            // Only the member the trajectory follows (same c) is tested.
                            double ct = p.cs[i];
                            if (std::fabs(ct - f.c) > 0.05 * (1 + std::fabs(f.c))) continue;
                        }
                        if (!fa_.familyDirection(px, py, f.c, tx2, ty2)) { hits.push_back({1, 0, 0, false}); continue; }
                        double cosv = std::fabs(tx1 * tx2 + ty1 * ty2) / std::hypot(tx1, ty1);
                        Env env(px, py, f.c);
#ifdef OT_DEBUG
                        if (cosv > 0.04) std::fprintf(stderr, "cos=%.3f at (%.3f,%.3f) c=%g bend %.3f fc=%g\n", cosv, px, py, f.c, b1, fa_.Fc->eval(Env(px, py, f.c)));
#endif
                        hits.push_back({cosv, std::hypot(fa_.Fx->eval(env), fa_.Fy->eval(env)), std::fabs(fa_.Fc->eval(env)),
                                        b1 > 0.15 || b2 > 0.15});
                    }
        }

    OrthoCheck r;
    if (hits.empty()) return r;
    auto median = [&](double Hit::*field) {
        std::vector<double> v;
        for (const Hit& h : hits) v.push_back(h.*field);
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    // Typical size of |dF/dc| over the whole view (not only at the crossings).
    std::vector<double> fcSample;
    for (int k = 1; k <= 200 && !fam.empty(); ++k) {
        Env env(view_.xmin + halton(k, 2) * view_.width(), view_.ymin + halton(k, 3) * view_.height(),
                fam[k % fam.size()].c);
        double v = std::fabs(fa_.Fc->eval(env));
        if (std::isfinite(v)) fcSample.push_back(v);
    }
    std::sort(fcSample.begin(), fcSample.end());
    const double medGrad = median(&Hit::grad);
    const double medFc = fcSample.empty() ? median(&Hit::fc) : fcSample[fcSample.size() / 2];
    double sum = 0;
    for (const Hit& h : hits) {
        // Skipped crossings:
        //  (1) the gradient (Fx, Fy) almost vanishes: the slope is undefined there;
        //  (2) dF/dc almost vanishes: (nearly) every curve of the family passes
        //      through this point (e.g. the origin for y = cx^2), so c and the
        //      direction of the trajectory are not determined there;
        //  (3) sharp bend: the drawn polyline is too coarse to measure a tangent.
        bool singular = !(h.grad > 1e-3 * medGrad) || !(h.fc > 0.02 * medFc);
        if (singular || h.coarse) { ++r.skippedSingular; continue; }
        ++r.crossings;
        sum += h.cosv;
        r.maxAbsCos = std::max(r.maxAbsCos, h.cosv);
    }
    if (r.crossings > 0) r.meanAbsCos = sum / r.crossings;
    return r;
}

}  // namespace ot
