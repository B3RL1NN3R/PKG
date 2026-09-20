#include "ColorConverter.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double Epsilon = 1e-9;
}

double ColorConverter::clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

int ColorConverter::clampChannel(int value)
{
    return std::max(0, std::min(value, 255));
}

double ColorConverter::normalizeHue(double hue)
{
    double result = std::fmod(hue, 360.0);
    if (result < 0.0)
        result += 360.0;
    return result;
}

RgbConversionResult ColorConverter::makeRgbResult(double r, double g, double b,
                                                   bool sourceWasClamped)
{
    const bool outOfRange = r < 0.0 || r > 255.0 ||
                            g < 0.0 || g > 255.0 ||
                            b < 0.0 || b > 255.0;

    r = clamp(r, 0.0, 255.0);
    g = clamp(g, 0.0, 255.0);
    b = clamp(b, 0.0, 255.0);

    const bool rounded = std::abs(r - std::round(r)) > Epsilon ||
                         std::abs(g - std::round(g)) > Epsilon ||
                         std::abs(b - std::round(b)) > Epsilon;

    return {
        {
            clampChannel(static_cast<int>(std::round(r))),
            clampChannel(static_cast<int>(std::round(g))),
            clampChannel(static_cast<int>(std::round(b)))
        },
        rounded,
        sourceWasClamped || outOfRange
    };
}

CmykColor ColorConverter::rgbToCmyk(const RgbColor& rgb)
{
    const double r = clamp(rgb.r, 0, 255) / 255.0;
    const double g = clamp(rgb.g, 0, 255) / 255.0;
    const double b = clamp(rgb.b, 0, 255) / 255.0;

    const double k = 1.0 - std::max({r, g, b});

    if (std::abs(k - 1.0) < Epsilon)
        return {0.0, 0.0, 0.0, 100.0};

    const double c = (1.0 - r - k) / (1.0 - k);
    const double m = (1.0 - g - k) / (1.0 - k);
    const double y = (1.0 - b - k) / (1.0 - k);

    return {c * 100.0, m * 100.0, y * 100.0, k * 100.0};
}

RgbConversionResult ColorConverter::cmykToRgb(const CmykColor& cmyk)
{
    const bool clamped = !std::isfinite(cmyk.c) || !std::isfinite(cmyk.m) ||
                         !std::isfinite(cmyk.y) || !std::isfinite(cmyk.k) ||
                         cmyk.c < 0.0 || cmyk.c > 100.0 ||
                         cmyk.m < 0.0 || cmyk.m > 100.0 ||
                         cmyk.y < 0.0 || cmyk.y > 100.0 ||
                         cmyk.k < 0.0 || cmyk.k > 100.0;

    const double c = clamp(cmyk.c, 0.0, 100.0) / 100.0;
    const double m = clamp(cmyk.m, 0.0, 100.0) / 100.0;
    const double y = clamp(cmyk.y, 0.0, 100.0) / 100.0;
    const double k = clamp(cmyk.k, 0.0, 100.0) / 100.0;

    return makeRgbResult(
        255.0 * (1.0 - c) * (1.0 - k),
        255.0 * (1.0 - m) * (1.0 - k),
        255.0 * (1.0 - y) * (1.0 - k),
        clamped
    );
}

HlsColor ColorConverter::rgbToHls(const RgbColor& rgb)
{
    const double r = clamp(rgb.r, 0, 255) / 255.0;
    const double g = clamp(rgb.g, 0, 255) / 255.0;
    const double b = clamp(rgb.b, 0, 255) / 255.0;

    const double maximum = std::max({r, g, b});
    const double minimum = std::min({r, g, b});
    const double delta = maximum - minimum;
    const double lightness = (maximum + minimum) / 2.0;

    if (delta < Epsilon)
        return {0.0, lightness * 100.0, 0.0};

    double hue;
    if (std::abs(maximum - r) < Epsilon)
        hue = 60.0 * std::fmod((g - b) / delta, 6.0);
    else if (std::abs(maximum - g) < Epsilon)
        hue = 60.0 * ((b - r) / delta + 2.0);
    else
        hue = 60.0 * ((r - g) / delta + 4.0);

    hue = normalizeHue(hue);
    const double saturation = delta / (1.0 - std::abs(2.0 * lightness - 1.0));

    return {hue, lightness * 100.0, saturation * 100.0};
}

RgbConversionResult ColorConverter::hlsToRgb(const HlsColor& hls)
{
    const bool clamped = !std::isfinite(hls.h) || !std::isfinite(hls.l) ||
                         !std::isfinite(hls.s) ||
                         hls.h < 0.0 || hls.h > 360.0 ||
                         hls.l < 0.0 || hls.l > 100.0 ||
                         hls.s < 0.0 || hls.s > 100.0;

    const double h = normalizeHue(clamp(hls.h, 0.0, 360.0));
    const double l = clamp(hls.l, 0.0, 100.0) / 100.0;
    const double s = clamp(hls.s, 0.0, 100.0) / 100.0;

    const double chroma = (1.0 - std::abs(2.0 * l - 1.0)) * s;
    const double x = chroma * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double offset = l - chroma / 2.0;

    double r1 = 0.0;
    double g1 = 0.0;
    double b1 = 0.0;

    if (h < 60.0)
        r1 = chroma, g1 = x;
    else if (h < 120.0)
        r1 = x, g1 = chroma;
    else if (h < 180.0)
        g1 = chroma, b1 = x;
    else if (h < 240.0)
        g1 = x, b1 = chroma;
    else if (h < 300.0)
        r1 = x, b1 = chroma;
    else
        r1 = chroma, b1 = x;

    return makeRgbResult(
        (r1 + offset) * 255.0,
        (g1 + offset) * 255.0,
        (b1 + offset) * 255.0,
        clamped
    );
}
