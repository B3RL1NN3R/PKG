#include "ColorController.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr double Epsilon = 1e-9;

double roundToOneDecimal(double value)
{
    return std::round(value * 10.0) / 10.0;
}

bool needsDisplayRounding(double value)
{
    return std::abs(value - roundToOneDecimal(value)) > Epsilon;
}

void appendMessage(std::string& message, const std::string& addition)
{
    if (!message.empty())
        message += " ";
    message += addition;
}

void reportDisplayRounding(const ColorState& state, std::string& message)
{
    const bool cmyk = needsDisplayRounding(state.cmyk.c) ||
                      needsDisplayRounding(state.cmyk.m) ||
                      needsDisplayRounding(state.cmyk.y) ||
                      needsDisplayRounding(state.cmyk.k);
    const bool hls = needsDisplayRounding(state.hls.h) ||
                     needsDisplayRounding(state.hls.l) ||
                     needsDisplayRounding(state.hls.s);
    if (cmyk || hls)
    {
        const std::string models = cmyk && hls ? "CMYK и HLS" : (cmyk ? "CMYK" : "HLS");
        appendMessage(message, "Рассчитанные значения " + models +
                               " показаны с округлением до 0,1.");
    }
}
}

ColorController::ColorController(UpdateHandler handler)
    : handler_(std::move(handler))
{
    state_.rgb = {255, 0, 0};
    state_.cmyk = ColorConverter::rgbToCmyk(state_.rgb);
    state_.hls = ColorConverter::rgbToHls(state_.rgb);
}

void ColorController::setRgb(const RgbColor& rgb)
{
    const RgbColor safe {
        std::clamp(rgb.r, 0, 255),
        std::clamp(rgb.g, 0, 255),
        std::clamp(rgb.b, 0, 255)
    };

    const bool clamped = safe.r != rgb.r || safe.g != rgb.g || safe.b != rgb.b;
    updateFromRgb(safe, clamped ? "Значения RGB ограничены допустимым диапазоном." : "");
}

void ColorController::setCmyk(const CmykColor& cmyk)
{
    if (!std::isfinite(cmyk.c) || !std::isfinite(cmyk.m) ||
        !std::isfinite(cmyk.y) || !std::isfinite(cmyk.k))
    {
        if (handler_)
            handler_({state_, "Компоненты CMYK должны быть конечными числами. Цвет не изменён."});
        return;
    }

    const bool clamped = cmyk.c < 0.0 || cmyk.c > 100.0 ||
                         cmyk.m < 0.0 || cmyk.m > 100.0 ||
                         cmyk.y < 0.0 || cmyk.y > 100.0 ||
                         cmyk.k < 0.0 || cmyk.k > 100.0;

    const CmykColor limited {
        std::clamp(cmyk.c, 0.0, 100.0),
        std::clamp(cmyk.m, 0.0, 100.0),
        std::clamp(cmyk.y, 0.0, 100.0),
        std::clamp(cmyk.k, 0.0, 100.0)
    };
    const CmykColor safe {
        roundToOneDecimal(limited.c),
        roundToOneDecimal(limited.m),
        roundToOneDecimal(limited.y),
        roundToOneDecimal(limited.k)
    };
    const RgbConversionResult result = ColorConverter::cmykToRgb(safe);
    const bool inputRounded = std::abs(safe.c - limited.c) > Epsilon ||
                              std::abs(safe.m - limited.m) > Epsilon ||
                              std::abs(safe.y - limited.y) > Epsilon ||
                              std::abs(safe.k - limited.k) > Epsilon;

    std::string message;
    if (clamped)
        message = "Значения CMYK ограничены диапазоном 0-100%.";
    if (inputRounded)
        appendMessage(message, "Значение CMYK округлено до одного знака после запятой.");
    if (result.rounded)
        appendMessage(message, "RGB округлён до целых каналов для отображения цвета.");

    state_.cmyk = safe;
    state_.rgb = result.color;
    state_.hls = ColorConverter::rgbToHls(result.color);
    reportDisplayRounding(state_, message);

    if (handler_)
        handler_({state_, message});
}

void ColorController::setHls(const HlsColor& hls)
{
    if (!std::isfinite(hls.h) || !std::isfinite(hls.l) || !std::isfinite(hls.s))
    {
        if (handler_)
            handler_({state_, "Компоненты HLS должны быть конечными числами. Цвет не изменён."});
        return;
    }

    const bool clamped = hls.h < 0.0 || hls.h > 360.0 ||
                         hls.l < 0.0 || hls.l > 100.0 ||
                         hls.s < 0.0 || hls.s > 100.0;

    const HlsColor limited {
        std::clamp(hls.h, 0.0, 360.0),
        std::clamp(hls.l, 0.0, 100.0),
        std::clamp(hls.s, 0.0, 100.0)
    };
    const HlsColor safe {
        roundToOneDecimal(limited.h),
        roundToOneDecimal(limited.l),
        roundToOneDecimal(limited.s)
    };
    const RgbConversionResult result = ColorConverter::hlsToRgb(safe);
    const bool inputRounded = std::abs(safe.h - limited.h) > Epsilon ||
                              std::abs(safe.l - limited.l) > Epsilon ||
                              std::abs(safe.s - limited.s) > Epsilon;

    std::string message;
    if (clamped)
        message = "Значения HLS ограничены допустимым диапазоном.";
    if (inputRounded)
        appendMessage(message, "Значение HLS округлено до одного знака после запятой.");
    if (result.rounded)
        appendMessage(message, "RGB округлён до целых каналов для отображения цвета.");

    state_.hls = safe;
    state_.rgb = result.color;
    state_.cmyk = ColorConverter::rgbToCmyk(result.color);
    reportDisplayRounding(state_, message);

    if (handler_)
        handler_({state_, message});
}

const ColorState& ColorController::state() const
{
    return state_;
}

void ColorController::updateFromRgb(const RgbColor& rgb, const std::string& message)
{
    state_.rgb = rgb;
    state_.cmyk = ColorConverter::rgbToCmyk(rgb);
    state_.hls = ColorConverter::rgbToHls(rgb);

    std::string notice = message;
    reportDisplayRounding(state_, notice);
    if (handler_)
        handler_({state_, notice});
}
