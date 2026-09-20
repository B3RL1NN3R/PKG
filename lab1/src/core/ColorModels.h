#pragma once

struct RgbColor
{
    int r;
    int g;
    int b;
};

struct CmykColor
{
    double c;
    double m;
    double y;
    double k;
};

struct HlsColor
{
    double h;
    double l;
    double s;
};

struct RgbConversionResult
{
    RgbColor color;
    bool rounded;
    bool clamped;
};
