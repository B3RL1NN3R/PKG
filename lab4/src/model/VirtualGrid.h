#pragma once
#include "../raster/IRasterTarget.h"
#include <vector>
#include <cstdint>

class VirtualGrid final : public IRasterTarget {
public:
    VirtualGrid(int width = 101, int height = 81);
    void Resize(int width, int height);
    void Clear();
    void SetPixel(int x, int y, double intensity = 1.0) override;
    double At(int x, int y) const;
    int Width() const;
    int Height() const;
    const std::vector<double>& Data() const;
private:
    int width_;
    int height_;
    std::vector<double> pixels_;
};
