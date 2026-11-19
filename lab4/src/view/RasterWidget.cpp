#include "RasterWidget.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QFont>
#include <QRectF>
#include <QMouseEvent>
#include <QKeyEvent>

namespace {
int firstMultipleAtOrAbove(int value, int step) {
    return static_cast<int>(std::ceil(static_cast<double>(value) / step)) * step;
}
}

RasterWidget::RasterWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(500, 420);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void RasterWidget::SetGrid(const VirtualGrid* grid) { grid_ = grid; update(); }

void RasterWidget::SetScale(int scale) {
    const int bounded = std::clamp(scale, 2, 40);
    if (scale_ == bounded) return;
    scale_ = bounded;
    update();
    emit ScaleChanged(scale_);
}

int RasterWidget::Scale() const { return scale_; }
void RasterWidget::ZoomIn() { SetScale(scale_ + 2); }
void RasterWidget::ZoomOut() { SetScale(scale_ - 2); }
void RasterWidget::ResetScale() {
    originOffset_ = QPointF(0.0, 0.0);
    SetScale(12);
    update();
}
QSize RasterWidget::minimumSizeHint() const { return QSize(500, 420); }

void RasterWidget::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    if (delta == 0) { event->accept(); return; }
    const int nextScale = std::clamp(scale_ + (delta > 0 ? 2 : -2), 2, 40);
    if (nextScale == scale_) { event->accept(); return; }
    const QRectF plot = QRectF(rect()).adjusted(42.0, 34.0, -42.0, -34.0);
    const QPointF center = plot.center() + originOffset_;
    const QPointF cursor = event->position();
    const qreal gridX = (cursor.x() - center.x()) / scale_;
    const qreal gridY = (center.y() - cursor.y()) / scale_;
    const qreal nextOriginX = cursor.x() - gridX * nextScale;
    const qreal nextOriginY = cursor.y() + gridY * nextScale;
    originOffset_ = QPointF(nextOriginX - plot.center().x(), nextOriginY - plot.center().y());
    scale_ = nextScale;
    update();
    emit ScaleChanged(scale_);
    event->accept();
}

void RasterWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPanPosition_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void RasterWidget::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPointF current = event->position();
        originOffset_ += current - lastPanPosition_;
        lastPanPosition_ = current;
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void RasterWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton)) {
        panning_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void RasterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(rect(), QColor(QStringLiteral("#f4f7fb")));

    const QRectF plot = QRectF(rect()).adjusted(42.0, 34.0, -42.0, -34.0);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    const qreal cx = plot.center().x() + originOffset_.x();
    const qreal cy = plot.center().y() + originOffset_.y();
    const qreal cell = scale_;
    const int minX = static_cast<int>(std::floor((plot.left() - cx) / cell - 0.5));
    const int maxX = static_cast<int>(std::ceil((plot.right() - cx) / cell + 0.5));
    const int minY = static_cast<int>(std::floor((cy - plot.bottom()) / cell - 0.5));
    const int maxY = static_cast<int>(std::ceil((cy - plot.top()) / cell + 0.5));

    p.save();
    p.setClipRect(plot);
    p.fillRect(plot, QColor(QStringLiteral("#ffffff")));

    const int majorStep = scale_ >= 12 ? 5 : (scale_ >= 7 ? 10 : 20);
    p.setPen(QPen(QColor(QStringLiteral("#edf1f6")), 1));
    for (int x = minX; x <= maxX; ++x) {
        if (x % majorStep == 0) continue;
        const qreal px = cx + (x + 0.5) * cell;
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
    }
    for (int y = minY; y <= maxY; ++y) {
        if (y % majorStep == 0) continue;
        const qreal py = cy - (y + 0.5) * cell;
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
    }

    p.setPen(QPen(QColor(QStringLiteral("#d8e0ea")), 1));
    for (int x = minX; x <= maxX; ++x) {
        if (x % majorStep != 0) continue;
        const qreal px = cx + (x + 0.5) * cell;
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
    }
    for (int y = minY; y <= maxY; ++y) {
        if (y % majorStep != 0) continue;
        const qreal py = cy - (y + 0.5) * cell;
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
    }

    if (grid_) {
        const int halfW = grid_->Width() / 2;
        const int halfH = grid_->Height() / 2;
        const auto& pixels = grid_->Data();
        for (int iy = 0; iy < grid_->Height(); ++iy) {
            const int y = halfH - iy;
            const qreal py = cy - y * cell;
            if (py + cell / 2 < plot.top() || py - cell / 2 > plot.bottom()) continue;
            for (int ix = 0; ix < grid_->Width(); ++ix) {
                const double intensity = pixels[static_cast<size_t>(iy) * grid_->Width() + ix];
                if (intensity <= 0.0) continue;
                const int x = ix - halfW;
                const qreal px = cx + x * cell;
                if (px + cell / 2 < plot.left() || px - cell / 2 > plot.right()) continue;
                QColor pixel(QStringLiteral("#2878d0"));
                pixel.setAlphaF(std::clamp(intensity, 0.0, 1.0));
                p.fillRect(QRectF(px - cell / 2 + 0.6, py - cell / 2 + 0.6,
                                  std::max(0.0, cell - 1.2), std::max(0.0, cell - 1.2)), pixel);
            }
        }
    }

    p.setPen(QPen(QColor(QStringLiteral("#52657d")), 1.8));
    p.drawLine(QPointF(plot.left(), cy), QPointF(plot.right() - 7, cy));
    p.drawLine(QPointF(cx, plot.bottom()), QPointF(cx, plot.top() + 7));
    p.drawLine(QPointF(plot.right() - 7, cy), QPointF(plot.right() - 13, cy - 4));
    p.drawLine(QPointF(plot.right() - 7, cy), QPointF(plot.right() - 13, cy + 4));
    p.drawLine(QPointF(cx, plot.top() + 7), QPointF(cx - 4, plot.top() + 13));
    p.drawLine(QPointF(cx, plot.top() + 7), QPointF(cx + 4, plot.top() + 13));
    p.setPen(QPen(QColor(QStringLiteral("#52657d")), 1.2));
    const int tickStep = majorStep;
    for (int x = firstMultipleAtOrAbove(minX, tickStep); x <= maxX; x += tickStep) {
        const qreal px = cx + x * cell;
        p.drawLine(QPointF(px, cy - 4), QPointF(px, cy + 4));
    }
    for (int y = firstMultipleAtOrAbove(minY, tickStep); y <= maxY; y += tickStep) {
        const qreal py = cy - y * cell;
        p.drawLine(QPointF(cx - 4, py), QPointF(cx + 4, py));
    }
    p.restore();

    p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
    p.setPen(QColor(QStringLiteral("#64748b")));
    const int labelStep = majorStep;
    for (int x = firstMultipleAtOrAbove(minX, labelStep); x <= maxX; x += labelStep) {
        if (x == 0) continue;
        const qreal px = cx + x * cell;
        if (px < plot.left() + 12 || px > plot.right() - 12) continue;
        p.drawText(QRectF(px - 18, cy + 7, 36, 16), Qt::AlignHCenter | Qt::AlignTop, QString::number(x));
    }
    for (int y = firstMultipleAtOrAbove(minY, labelStep); y <= maxY; y += labelStep) {
        if (y == 0) continue;
        const qreal py = cy - y * cell;
        if (py < plot.top() + 8 || py > plot.bottom() - 8) continue;
        p.drawText(QRectF(cx + 7, py - 8, 34, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::number(y));
    }
    p.setPen(QColor(QStringLiteral("#334155")));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
    p.drawText(QRectF(plot.right() - 18, cy - 24, 18, 18), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("X"));
    p.drawText(QRectF(cx + 7, plot.top() + 2, 18, 18), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("Y"));
    p.drawText(QRectF(cx + 7, cy + 5, 22, 16), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("0"));

}

void RasterWidget::keyPressEvent(QKeyEvent* event) {
    const qreal step = 30.0;
    switch (event->key()) {
    case Qt::Key_Left: originOffset_.rx() += step; break;
    case Qt::Key_Right: originOffset_.rx() -= step; break;
    case Qt::Key_Up: originOffset_.ry() += step; break;
    case Qt::Key_Down: originOffset_.ry() -= step; break;
    case Qt::Key_Plus: case Qt::Key_Equal: ZoomIn(); event->accept(); return;
    case Qt::Key_Minus: ZoomOut(); event->accept(); return;
    case Qt::Key_0: ResetScale(); event->accept(); return;
    default: QWidget::keyPressEvent(event); return;
    }
    update();
    event->accept();
}
