#pragma once

#include "core/ColorConverter.h"

#include <functional>
#include <string>

struct ColorState
{
    RgbColor rgb;
    CmykColor cmyk;
    HlsColor hls;
};

struct ColorUpdate
{
    ColorState state;
    std::string message;
};

class ColorController
{
public:
    using UpdateHandler = std::function<void(const ColorUpdate&)>;

    explicit ColorController(UpdateHandler handler);

    void setRgb(const RgbColor& rgb);
    void setCmyk(const CmykColor& cmyk);
    void setHls(const HlsColor& hls);

    const ColorState& state() const;

private:
    void updateFromRgb(const RgbColor& rgb, const std::string& message);

    ColorState state_{};
    UpdateHandler handler_;
};
