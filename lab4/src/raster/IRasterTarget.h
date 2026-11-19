#pragma once

class IRasterTarget {
public:
    virtual ~IRasterTarget() = default;
    virtual void SetPixel(int x, int y, double intensity = 1.0) = 0;
};
