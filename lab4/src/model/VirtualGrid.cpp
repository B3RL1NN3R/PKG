#include "VirtualGrid.h"
#include <algorithm>

VirtualGrid::VirtualGrid(int width, int height) : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height, 0.0) {}
void VirtualGrid::Resize(int width, int height) { width_ = width; height_ = height; pixels_.assign(static_cast<size_t>(width) * height, 0.0); }
void VirtualGrid::Clear() { std::fill(pixels_.begin(), pixels_.end(), 0.0); }
void VirtualGrid::SetPixel(int x, int y, double intensity) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    auto& value = pixels_[static_cast<size_t>(y) * width_ + x];
    value = std::max(value, std::clamp(intensity, 0.0, 1.0));
}
double VirtualGrid::At(int x, int y) const { return pixels_[static_cast<size_t>(y) * width_ + x]; }
int VirtualGrid::Width() const { return width_; }
int VirtualGrid::Height() const { return height_; }
const std::vector<double>& VirtualGrid::Data() const { return pixels_; }
