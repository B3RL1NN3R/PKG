#pragma once
#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>

class BenchmarkWidget final : public QWidget {
    Q_OBJECT
public:
    struct Series {
        QString name;
        QVector<double> x;
        QVector<double> y;
        QColor color;
    };

    explicit BenchmarkWidget(QWidget* parent = nullptr);
    void SetData(const QVector<Series>& lineCount, const QVector<Series>& lineLength,
                 const QVector<Series>& circleCount, const QVector<Series>& circleRadius);
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QVector<Series> lineCount_;
    QVector<Series> lineLength_;
    QVector<Series> circleCount_;
    QVector<Series> circleRadius_;
};
