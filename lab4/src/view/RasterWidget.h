#pragma once
#include <QWidget>
#include <QPointF>
#include "../model/VirtualGrid.h"

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

class RasterWidget final : public QWidget {
    Q_OBJECT
public:
    explicit RasterWidget(QWidget* parent = nullptr);
    void SetGrid(const VirtualGrid* grid);
    void SetScale(int scale);
    int Scale() const;
    void ZoomIn();
    void ZoomOut();
    void ResetScale();
    QSize minimumSizeHint() const override;
signals:
    void ScaleChanged(int scale);
protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
private:
    const VirtualGrid* grid_ = nullptr;
    int scale_ = 12;
    QPointF originOffset_;
    QPointF lastPanPosition_;
    bool panning_ = false;
};
