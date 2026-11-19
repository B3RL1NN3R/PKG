#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QDialog>
#include <QTextEdit>
#include "RasterWidget.h"
#include "BenchmarkWidget.h"
#include "../model/VirtualGrid.h"

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private slots:
    void Draw();
    void RunBenchmark();
    void UpdateParameterVisibility();
private:
    VirtualGrid grid_;
    RasterWidget* raster_ = nullptr;
    BenchmarkWidget* chart_ = nullptr;
    QDialog* benchmarkDialog_ = nullptr;
    QTextEdit* steps_ = nullptr;
    QComboBox* algorithm_ = nullptr;
    QComboBox* primitive_ = nullptr;
    QSpinBox* x0_ = nullptr;
    QSpinBox* y0_ = nullptr;
    QSpinBox* x1_ = nullptr;
    QSpinBox* y1_ = nullptr;
    QSpinBox* radiusX_ = nullptr;
    QSpinBox* radiusY_ = nullptr;
    QSpinBox* scale_ = nullptr;
    QSpinBox* startAngle_ = nullptr;
    QSpinBox* endAngle_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* x0Label_ = nullptr;
    QLabel* y0Label_ = nullptr;
    QLabel* x1Label_ = nullptr;
    QLabel* y1Label_ = nullptr;
    QLabel* radiusXLabel_ = nullptr;
    QLabel* radiusYLabel_ = nullptr;
    QLabel* primitiveLabel_ = nullptr;
    QWidget* primitiveField_ = nullptr;
    QWidget* angleFields_ = nullptr;
    QWidget* lineFields_ = nullptr;
    QWidget* radiusXField_ = nullptr;
    QWidget* radiusYField_ = nullptr;
    bool updatingExample_ = false;
    void AddControls(QWidget* panel);
    void ShowAlgorithmExample();
};
