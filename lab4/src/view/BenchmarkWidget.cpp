#include "BenchmarkWidget.h"
#include <QPainter>
#include <QFont>
#include <QRectF>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

BenchmarkWidget::BenchmarkWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(900, 650);
}

void BenchmarkWidget::SetData(const QVector<Series>& lineCount, const QVector<Series>& lineLength,
                              const QVector<Series>& circleCount, const QVector<Series>& circleRadius) {
    lineCount_ = lineCount;
    lineLength_ = lineLength;
    circleCount_ = circleCount;
    circleRadius_ = circleRadius;
    update();
}

void BenchmarkWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(QStringLiteral("#f4f7fb")));

    auto drawChart = [&](QRect area, const QVector<Series>& series, const QString& title,
                         const QString& xTitle) {
        p.fillRect(area, QColor(QStringLiteral("#ffffff")));
        p.setPen(QColor(QStringLiteral("#344861")));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
        p.drawText(QRect(area.left() + 14, area.top() + 8, area.width() - 28, 22),
                   Qt::AlignLeft | Qt::AlignVCenter, title);

        QRect plot = area.adjusted(80, 58, -18, -58);
        if (series.isEmpty()) {
            p.setPen(QColor(QStringLiteral("#718096")));
            p.drawText(plot, Qt::AlignCenter, QStringLiteral("Нет данных"));
            return;
        }

        double maxX = 1.0;
        double maxY = 1.0;
        for (const Series& item : series) {
            for (double value : item.x) maxX = std::max(maxX, value);
            for (double value : item.y) maxY = std::max(maxY, value);
        }

        p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        p.setPen(QPen(QColor(QStringLiteral("#e5ebf2")), 1));
        for (int i = 0; i <= 4; ++i) {
            const qreal y = plot.top() + plot.height() * i / 4.0;
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            const double value = maxY * (4 - i) / 4.0;
            p.setPen(QColor(QStringLiteral("#64748b")));
            p.drawText(QRect(plot.left() - 60, int(y) - 8, 52, 16), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(value, 'g', 3));
            p.setPen(QPen(QColor(QStringLiteral("#e5ebf2")), 1));
        }
        for (int i = 0; i <= 4; ++i) {
            const qreal x = plot.left() + plot.width() * i / 4.0;
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            p.setPen(QColor(QStringLiteral("#64748b")));
            p.drawText(QRect(int(x) - 34, plot.bottom() + 3, 68, 15), Qt::AlignHCenter | Qt::AlignVCenter,
                       QString::number(maxX * i / 4.0, 'g', 3));
            p.setPen(QPen(QColor(QStringLiteral("#e5ebf2")), 1));
        }
        p.setPen(QPen(QColor(QStringLiteral("#9aa9ba")), 1));
        p.drawLine(plot.bottomLeft(), plot.topLeft());
        p.drawLine(plot.bottomLeft(), plot.bottomRight());

        for (const Series& item : series) {
            if (item.x.isEmpty() || item.x.size() != item.y.size()) continue;
            QPainterPath path;
            for (int i = 0; i < item.x.size(); ++i) {
                const qreal x = plot.left() + item.x[i] * plot.width() / maxX;
                const qreal y = plot.bottom() - item.y[i] * plot.height() / maxY;
                if (i == 0) path.moveTo(x, y);
                else path.lineTo(x, y);
            }
            p.setPen(QPen(item.color, 2.2));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setBrush(item.color);
            for (int i = 0; i < item.x.size(); ++i) {
                const qreal x = plot.left() + item.x[i] * plot.width() / maxX;
                const qreal y = plot.bottom() - item.y[i] * plot.height() / maxY;
                p.drawEllipse(QPointF(x, y), 3.0, 3.0);
            }
        }

        p.setFont(QFont(QStringLiteral("Segoe UI"), 8));
        int legendX = plot.left();
        const int legendY = area.top() + 34;
        for (const Series& item : series) {
            p.setPen(QPen(item.color, 2.5));
            p.drawLine(legendX, legendY + 7, legendX + 18, legendY + 7);
            p.setPen(QColor(QStringLiteral("#53657a")));
            const int textWidth = p.fontMetrics().horizontalAdvance(item.name);
            p.drawText(legendX + 23, legendY + 12, item.name);
            legendX += textWidth + 42;
        }
        p.setPen(QColor(QStringLiteral("#53657a")));
        p.drawText(QRect(plot.left(), area.bottom() - 20, plot.width(), 16),
                   Qt::AlignRight | Qt::AlignVCenter, xTitle);
        p.save();
        p.translate(area.left() + 8, plot.center().y());
        p.rotate(-90);
        p.drawText(QRect(-plot.height() / 2, -8, plot.height(), 16), Qt::AlignCenter,
                   QStringLiteral("Время, мс"));
        p.restore();
    };

    const int gap = 12;
    const int halfW = (width() - gap) / 2;
    const int halfH = (height() - gap) / 2;
    drawChart(QRect(0, 0, halfW, halfH), lineCount_,
              QStringLiteral("Время от количества отрезков"), QStringLiteral("Количество отрезков"));
    drawChart(QRect(halfW + gap, 0, width() - halfW - gap, halfH), lineLength_,
              QStringLiteral("Время от длины отрезка"), QStringLiteral("Длина отрезка, пикселей"));
    drawChart(QRect(0, halfH + gap, halfW, height() - halfH - gap), circleCount_,
              QStringLiteral("Время от количества окружностей"), QStringLiteral("Количество окружностей"));
    drawChart(QRect(halfW + gap, halfH + gap, width() - halfW - gap, height() - halfH - gap), circleRadius_,
              QStringLiteral("Время от радиуса окружности"), QStringLiteral("Радиус, пикселей"));
}
