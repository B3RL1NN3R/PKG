#pragma once

#include "ColorModels.h"

class ColorConverter
{
public:
    static CmykColor rgbToCmyk(const RgbColor& rgb);
    static RgbConversionResult cmykToRgb(const CmykColor& cmyk);

    static HlsColor rgbToHls(const RgbColor& rgb);
    static RgbConversionResult hlsToRgb(const HlsColor& hls);

private:
    static double clamp(double value, double minimum, double maximum);
    static int clampChannel(int value);
    static double normalizeHue(double hue);
    static RgbConversionResult makeRgbResult(double r, double g, double b,
                                             bool sourceWasClamped);
};
