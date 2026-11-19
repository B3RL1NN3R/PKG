#include "RasterEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>

namespace {
int ipart(double x) { return static_cast<int>(std::floor(x)); }
double fpart(double x) { return x - std::floor(x); }
double rfpart(double x) { return 1.0 - fpart(x); }
void plotCircle8(IRasterTarget& t, int cx, int cy, int x, int y) {
    t.SetPixel(cx + x, cy + y); t.SetPixel(cx - x, cy + y);
    t.SetPixel(cx + x, cy - y); t.SetPixel(cx - x, cy - y);
    t.SetPixel(cx + y, cy + x); t.SetPixel(cx - y, cy + x);
    t.SetPixel(cx + y, cy - x); t.SetPixel(cx - y, cy - x);
}
}

namespace RasterEngine {
void StepLine(IRasterTarget& target, int x0, int y0, int x1, int y1) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) { target.SetPixel(x0, y0); return; }
    const int sx = dx < 0 ? -1 : 1;
    const int sy = dy < 0 ? -1 : 1;
    double x = x0;
    double y = y0;
    if (std::abs(dx) >= std::abs(dy)) {
        const double slope = static_cast<double>(dy) / std::abs(dx);
        for (int i = 0; i <= steps; ++i) {
            target.SetPixel(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
            x += sx;
            y += slope;
        }
    } else {
        const double inverseSlope = static_cast<double>(dx) / std::abs(dy);
        for (int i = 0; i <= steps; ++i) {
            target.SetPixel(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
            y += sy;
            x += inverseSlope;
        }
    }
}

void DDALine(IRasterTarget& target, int x0, int y0, int x1, int y1) {
    double dx = static_cast<double>(x1) - x0, dy = static_cast<double>(y1) - y0;
    int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy)));
    if (steps == 0) { target.SetPixel(x0, y0); return; }
    double sx = dx / steps, sy = dy / steps, x = x0, y = y0;
    for (int i = 0; i <= steps; ++i) { target.SetPixel(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))); x += sx; y += sy; }
}

void BresenhamLine(IRasterTarget& target, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        target.SetPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void BresenhamCircle(IRasterTarget& target, int cx, int cy, int radius) {
    if (radius < 0) return;
    int x = 0, y = radius, d = 3 - 2 * radius;
    while (x <= y) {
        plotCircle8(target, cx, cy, x, y);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; --y; }
        ++x;
    }
}

void BresenhamEllipse(IRasterTarget& target, int cx, int cy, int rx, int ry) {
    if (rx < 0 || ry < 0) return;
    if (rx == 0) { for (int y = -ry; y <= ry; ++y) target.SetPixel(cx, cy + y); return; }
    if (ry == 0) { for (int x = -rx; x <= rx; ++x) target.SetPixel(cx + x, cy); return; }
    const std::int64_t rx2 = 1LL * rx * rx;
    const std::int64_t ry2 = 1LL * ry * ry;
    std::int64_t x = 0;
    std::int64_t y = ry;
    std::int64_t dx = 0;
    std::int64_t dy = 2 * rx2 * y;
    std::int64_t decision1 = ry2 - rx2 * ry + rx2 / 4;
    auto plot = [&](std::int64_t px, std::int64_t py) {
        target.SetPixel(cx + static_cast<int>(px), cy + static_cast<int>(py));
        target.SetPixel(cx - static_cast<int>(px), cy + static_cast<int>(py));
        target.SetPixel(cx + static_cast<int>(px), cy - static_cast<int>(py));
        target.SetPixel(cx - static_cast<int>(px), cy - static_cast<int>(py));
    };
    while (dx < dy) {
        plot(x, y);
        ++x;
        dx += 2 * ry2;
        if (decision1 < 0) decision1 += dx + ry2;
        else { --y; dy -= 2 * rx2; decision1 += dx - dy + ry2; }
    }
    std::int64_t decision2 = ry2 * (x * x + x) + ry2 / 4 + rx2 * (y - 1) * (y - 1) - rx2 * ry2;
    while (y >= 0) {
        plot(x, y);
        --y;
        dy -= 2 * rx2;
        if (decision2 > 0) decision2 += rx2 - dy;
        else { ++x; dx += 2 * ry2; decision2 += dx - dy + rx2; }
    }
}

void WuLine(IRasterTarget& target, int x0, int y0, int x1, int y1) {
    if (x0 == x1 && y0 == y1) {
        target.SetPixel(x0, y0, 1.0);
        return;
    }
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    double dx = x1 - x0, dy = y1 - y0;
    double gradient = dx == 0.0 ? 1.0 : dy / dx;
    auto plot = [&](int x, int y, double a) { if (steep) target.SetPixel(y, x, std::clamp(a, 0.0, 1.0)); else target.SetPixel(x, y, std::clamp(a, 0.0, 1.0)); };
    double xend = std::round(x0), yend = y0 + gradient * (xend - x0), xgap = rfpart(x0 + 0.5);
    int xpxl1 = static_cast<int>(xend), ypxl1 = ipart(yend);
    plot(xpxl1, ypxl1, rfpart(yend) * xgap); plot(xpxl1, ypxl1 + 1, fpart(yend) * xgap);
    double intery = yend + gradient;
    xend = std::round(x1); yend = y1 + gradient * (xend - x1); xgap = fpart(x1 + 0.5);
    int xpxl2 = static_cast<int>(xend), ypxl2 = ipart(yend);
    for (int x = xpxl1 + 1; x < xpxl2; ++x) { plot(x, ipart(intery), rfpart(intery)); plot(x, ipart(intery) + 1, fpart(intery)); intery += gradient; }
    plot(xpxl2, ypxl2, rfpart(yend) * xgap); plot(xpxl2, ypxl2 + 1, fpart(yend) * xgap);
}

namespace {
int getOctant(std::int64_t gx, std::int64_t gy) {
    int oct = 0;
    if (gx < 0) { oct = 7; gx = -gx; }
    if (gy > 0) { oct ^= 3; gy = -gy; }
    if (gx > -gy) oct ^= 1;
    return ++oct;
}
void pittewayConic(IRasterTarget& target, int xs, int ys, std::int64_t A, std::int64_t B, std::int64_t C, std::int64_t D, std::int64_t E, std::int64_t F) {
    int octant = getOctant(D, E);
    int octantCount = 8;
    int dxsquare=0, dysquare=0, dxdiag=1, dydiag=1;
    if ((octant + 1) & 4) { D=-D; dxdiag=-1; }
    if ((octant - 1) & 4) { E=-E; dydiag=-1; }
    if ((octant - 1) & 2) B=-B;
    if (octant & 2) { dysquare=dydiag; std::swap(A,C); std::swap(D,E); }
    else dxsquare=dxdiag;
    A*=4; B*=4; C*=4; D*=4; E*=4; F*=4;
    std::int64_t d=A+B/2+C/4+D+E/2+F;
    std::int64_t u=A+B/2+D;
    std::int64_t v=A+B/2+D+E;
    std::int64_t k1=2*A, k2=2*A+B, k3=2*A+2*B+2*C;
    if (!(octant & 1)) { d=-d; k1=-k1; u=-u; k2=-k2; v=-v; k3=-k3; }
    int x=xs, y=ys;
    std::int64_t safety=0;
    for (;;) {
        while ((u > 0 || (octant & 1)) && (v < 0 || (~octant & 1))) {
            target.SetPixel(x,y);
            if (d < 0) { x+=dxsquare; y+=dysquare; u+=k1; v+=k2; d+=u; }
            else { x+=dxdiag; y+=dydiag; u+=k2; v+=k3; d+=v; }
            if (++safety > 1000000) return;
        }
        if (--octantCount < 0) return;
        if (++octant & 1) {
            d=-d-u+v-k1+k2;
            v=-2*u+v-k1+k2;
            u=-u-k1+k2;
            k3=-4*k1+4*k2-k3;
            k2=-2*k1+k2;
            k1=-k1;
            int sw=dxdiag; dxdiag=-dydiag; dydiag=sw;
        } else {
            d=-d+u-v/2+k2/2-3*k3/8;
            u=u-v+k2/2-k3/2;
            v=-v+k2-k3/2;
            k1=-k1+2*k2-k3;
            k2=k2-k3;
            k3=-k3;
            int sw=dxsquare; dxsquare=-dysquare; dysquare=sw;
        }
    }
}
}
namespace {
class ConicFilterTarget final : public IRasterTarget {
public:
    ConicFilterTarget(IRasterTarget& target, int cx, int cy, RasterEngine::ConicType type,
                      double startAngle, double endAngle)
        : target_(target), cx_(cx), cy_(cy), type_(type), start_(normalize(startAngle)),
          span_(normalize(endAngle - startAngle)) {}

    void SetPixel(int x, int y, double intensity = 1.0) override {
        if (type_ == RasterEngine::ConicType::Hyperbola && x < cx_) return;
        if (type_ != RasterEngine::ConicType::Arc) {
            target_.SetPixel(x, y, intensity);
            return;
        }
        const double angle = normalize(std::atan2(cy_ - y, x - cx_) * 180.0 / 3.14159265358979323846);
        const double offset = normalize(angle - start_);
        const bool full = std::abs(span_) < 1e-9;
        if (full || offset <= span_ + 1e-9) target_.SetPixel(x, y, intensity);
    }
private:
    static double normalize(double value) {
        value = std::fmod(value, 360.0);
        return value < 0.0 ? value + 360.0 : value;
    }
    IRasterTarget& target_;
    int cx_;
    int cy_;
    RasterEngine::ConicType type_;
    double start_;
    double span_;
};

class MirrorYTarget final : public IRasterTarget {
public:
    MirrorYTarget(IRasterTarget& target, int axisY) : target_(target), axisY_(axisY) {}
    void SetPixel(int x, int y, double intensity = 1.0) override {
        target_.SetPixel(x, 2 * axisY_ - y, intensity);
    }
private:
    IRasterTarget& target_;
    int axisY_;
};
}

void PittewayConic(IRasterTarget& target, int cx, int cy, int a, int b, ConicType type,
                   double startAngleDegrees, double endAngleDegrees) {
    if (a <= 0) return;
    if ((type == ConicType::Ellipse || type == ConicType::Hyperbola || type == ConicType::Arc) && b <= 0) return;
    if (type == ConicType::Circle) b = a;

    ConicFilterTarget filtered(target, cx, cy, type, startAngleDegrees, endAngleDegrees);
    std::int64_t A = 0, B = 0, C = 0, D = 0, E = 0, F = 0;
    int seedX = cx;
    int seedY = cy;

    switch (type) {
    case ConicType::Ellipse:
    case ConicType::Arc:
    case ConicType::Circle: {
        A = 1LL * b * b;
        C = 1LL * a * a;
        D = 2LL * a * b * b;
        seedX = cx + a;
        break;
    }
    case ConicType::Hyperbola: {
        A = -1LL * b * b;
        C = 1LL * a * a;
        D = -2LL * a * b * b;
        seedX = cx + a;
        break;
    }
    case ConicType::Parabola: {
        C = 1;
        D = -4LL * a;
        E = -4LL * a;
        seedX = cx + a;
        seedY = cy - 2 * a;
        break;
    }
    }
    if (type == ConicType::Hyperbola) {
        A = -1LL * b * b;
        C = 1LL * a * a;
        D = -2LL * a * b * b;
        pittewayConic(filtered, cx + a, cy, A, B, C, D, E, F);
        MirrorYTarget mirrored(filtered, cy);
        pittewayConic(mirrored, cx + a, cy, A, B, C, D, E, F);
        return;
    }
    if (type == ConicType::Parabola) {
        pittewayConic(filtered, cx + a, cy - 2 * a, 0, 0, 1, -4LL * a, -4LL * a, 0);
        MirrorYTarget mirrored(filtered, cy);
        pittewayConic(mirrored, cx + a, cy - 2 * a, 0, 0, 1, -4LL * a, -4LL * a, 0);
        return;
    }
    pittewayConic(filtered, seedX, seedY, A, B, C, D, E, F);
}

void PittewayEllipse(IRasterTarget& target, int cx, int cy, int rx, int ry) {
    if (rx < 0 || ry < 0) return;
    if (rx == 0 || ry == 0) {
        if (rx == 0) for (int y = -ry; y <= ry; ++y) target.SetPixel(cx, cy + y);
        else for (int x = -rx; x <= rx; ++x) target.SetPixel(cx + x, cy);
        return;
    }
    PittewayConic(target, cx, cy, rx, ry, ConicType::Ellipse);
}
}
