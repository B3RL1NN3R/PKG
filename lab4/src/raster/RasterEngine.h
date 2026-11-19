#pragma once
#include "IRasterTarget.h"
#include <cstdint>

namespace RasterEngine {
void StepLine(IRasterTarget& target, int x0, int y0, int x1, int y1);
void DDALine(IRasterTarget& target, int x0, int y0, int x1, int y1);
void BresenhamLine(IRasterTarget& target, int x0, int y0, int x1, int y1);
void BresenhamCircle(IRasterTarget& target, int cx, int cy, int radius);
void BresenhamEllipse(IRasterTarget& target, int cx, int cy, int rx, int ry);
void WuLine(IRasterTarget& target, int x0, int y0, int x1, int y1);
enum class ConicType { Ellipse, Hyperbola, Parabola, Circle, Arc };
void PittewayConic(IRasterTarget& target, int cx, int cy, int a, int b, ConicType type,
                   double startAngleDegrees = 0.0, double endAngleDegrees = 360.0);
void PittewayEllipse(IRasterTarget& target, int cx, int cy, int rx, int ry);
}
