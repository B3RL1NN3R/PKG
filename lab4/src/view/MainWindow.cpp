#include "MainWindow.h"
#include "../raster/RasterEngine.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QStatusBar>
#include <QPoint>
#include <QPair>
#include <QScrollArea>
#include <QDialog>
#include <QTextEdit>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

namespace {
using LineFunction = void (*)(IRasterTarget&, int, int, int, int);


QPair<QPoint, QPoint> randomOctantLine(std::mt19937& rng, int length, int fixedOctant = -1) {
    std::uniform_int_distribution<int> octantDistribution(0, 7);
    const int octant = fixedOctant >= 0 ? fixedOctant % 8 : octantDistribution(rng);
    const int major = std::max(1, length);
    std::uniform_int_distribution<int> minorDistribution(0, major);
    const int minor = minorDistribution(rng);
    int dx = major;
    int dy = minor;
    if (octant == 1 || octant == 2 || octant == 5 || octant == 6) std::swap(dx, dy);
    if (octant == 2 || octant == 3 || octant == 6 || octant == 7) dx = -dx;
    if (octant == 4 || octant == 5 || octant == 6 || octant == 7) dy = -dy;
    const int x = 1024;
    const int y = 1024;
    return {QPoint(x, y), QPoint(x + dx, y + dy)};
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), grid_(101, 81) {
    setWindowTitle(QStringLiteral("ParserLab"));
    resize(1320, 860);
    setMinimumSize(980, 680);
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #f4f7fb; color: #243247; font-family: 'Segoe UI'; font-size: 10pt; }"
        "QGroupBox { background: #ffffff; border: 1px solid #dce4ee; border-radius: 12px; margin-top: 14px; padding: 14px 10px 10px 10px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; color: #344861; }"
        "QComboBox, QSpinBox { background: #ffffff; border: 1px solid #d4deea; border-radius: 8px; padding: 8px 10px; min-height: 22px; selection-background-color: #2878d0; }"
        "QComboBox { font-weight: 600; }"
        "QComboBox:focus, QSpinBox:focus { border: 1px solid #438ee0; }"
        "QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #d4deea; selection-background-color: #e7f0fb; selection-color: #243247; padding: 5px; }"
        "QPushButton { background: #2878d0; color: white; border: none; border-radius: 8px; padding: 10px 12px; font-weight: 600; }"
        "QPushButton:hover { background: #1f69bb; } QPushButton:pressed { background: #19599f; }"
        "QPushButton#secondary { background: #eaf1f8; color: #31506f; } QPushButton#secondary:hover { background: #dce8f4; }"
        "QScrollArea { border: none; background: transparent; }"
        "QStatusBar { background: #ffffff; border-top: 1px solid #dce4ee; color: #53657a; }"
    ));

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(14, 14, 14, 10);
    rootLayout->setSpacing(10);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("RasterLab"), root);
    title->setStyleSheet(QStringLiteral("font-size: 20pt; font-weight: 700; color: #1d3552;"));
    header->addWidget(title);
    header->addStretch();
    rootLayout->addLayout(header);

    auto* splitter = new QSplitter(Qt::Horizontal, root);
    splitter->setChildrenCollapsible(false);
    auto* scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(280);
    scroll->setMaximumWidth(360);
    auto* panel = new QWidget(scroll);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 8, 0);
    panelLayout->setSpacing(8);
    auto* canvasPane = new QWidget(splitter);
    auto* canvasLayout = new QVBoxLayout(canvasPane);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(8);
    raster_ = new RasterWidget(canvasPane);
    steps_ = new QTextEdit(canvasPane);
    steps_->setReadOnly(true);
    steps_->setMinimumHeight(115);
    steps_->setMaximumHeight(170);
    steps_->setPlaceholderText(QString());
    steps_->setStyleSheet(QStringLiteral("QTextEdit { background: #ffffff; border: 1px solid #dce4ee; border-radius: 10px; padding: 8px; color: #344861; font-family: Consolas; font-size: 9pt; }"));
    canvasLayout->addWidget(raster_, 1);
    canvasLayout->addWidget(steps_);
    AddControls(panel);
    scroll->setWidget(panel);
    splitter->addWidget(scroll);
    splitter->addWidget(canvasPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 900});
    rootLayout->addWidget(splitter, 1);

    auto* footer = new QHBoxLayout();
    auto* benchmarkToggle = new QPushButton(QStringLiteral("Бенчмарк"), root);
    benchmarkToggle->setObjectName(QStringLiteral("secondary"));
    footer->addWidget(benchmarkToggle);
    footer->addStretch();
    auto* hint = new QLabel(QStringLiteral("ЛКМ — перемещение · Колесо — масштаб · Стрелки — сдвиг"), root);
    hint->setStyleSheet(QStringLiteral("color: #718096; font-size: 9pt;"));
    footer->addWidget(hint);
    rootLayout->addLayout(footer);
    connect(benchmarkToggle, &QPushButton::clicked, this, &MainWindow::RunBenchmark);

    setCentralWidget(root);
    status_ = new QLabel(QStringLiteral("Готово"), this);
    statusBar()->addWidget(status_, 1);
    Draw();
}

void MainWindow::AddControls(QWidget* panel) {
    auto* layout = qobject_cast<QVBoxLayout*>(panel->layout());

    auto* algorithmGroup = new QGroupBox(QStringLiteral("Параметры построения"), panel);
    auto* algorithmLayout = new QVBoxLayout(algorithmGroup);
    algorithmLayout->setContentsMargins(10, 14, 10, 10);
    algorithmLayout->setSpacing(10);
    algorithmLayout->addWidget(new QLabel(QStringLiteral("Метод:"), algorithmGroup));
    algorithm_ = new QComboBox(algorithmGroup);
    algorithm_->setMinimumHeight(32);
    algorithm_->addItems({QStringLiteral("Пошаговый алгоритм"), QStringLiteral("ЦДА"),
                          QStringLiteral("Брезенхем"), QStringLiteral("Ву"),
                          QStringLiteral("Кастл—Питтвей")});
    algorithm_->setStyleSheet(QStringLiteral(
        "QComboBox { background-color: white; color: #2c3e50; border: 1px solid #bdc3c7; "
        "border-radius: 4px; padding: 4px 8px; font-weight: bold; } "
        "QComboBox:hover { border-color: #3498db; } "
        "QComboBox:focus { border-color: #2980b9; } "
        "QComboBox:disabled { background-color: #ecf0f1; color: #7f8c8d; } "
        "QComboBox::drop-down { border: none; background: transparent; } "
        "QComboBox QAbstractItemView { background-color: white; color: #2c3e50; "
        "selection-background-color: #3498db; selection-color: white; "
        "border: 1px solid #bdc3c7; outline: none; } "
        "QComboBox QAbstractItemView::item { background-color: white; color: #2c3e50; "
        "min-height: 24px; padding: 4px 8px; } "
        "QComboBox QAbstractItemView::item:hover, QComboBox QAbstractItemView::item:selected { "
        "background-color: #3498db; color: white; }"));
    algorithmLayout->addWidget(algorithm_);
    primitiveLabel_ = new QLabel(QStringLiteral("Фигура:"), algorithmGroup);
    primitive_ = new QComboBox(algorithmGroup);
    primitive_->setMinimumHeight(32);
    primitive_->setStyleSheet(algorithm_->styleSheet());
    algorithmLayout->addWidget(primitiveLabel_);
    algorithmLayout->addWidget(primitive_);
    primitiveField_ = primitive_;
    layout->addWidget(algorithmGroup);

    auto* paramsGroup = new QGroupBox(QStringLiteral("Параметры"), panel);
    auto* paramsLayout = new QVBoxLayout(paramsGroup);
    auto makeSpin = [&](int value, int min, int max) {
        auto* spin = new QSpinBox(paramsGroup);
        spin->setRange(min, max);
        spin->setValue(value);
        spin->setAlignment(Qt::AlignCenter);
        return spin;
    };

    auto* centerBox = new QWidget(paramsGroup);
    auto* centerForm = new QFormLayout(centerBox);
    centerForm->setContentsMargins(0, 0, 0, 0);
    centerForm->setSpacing(8);
    x0_ = makeSpin(-25, -50, 50);
    y0_ = makeSpin(-12, -40, 40);
    x0Label_ = new QLabel(centerBox);
    y0Label_ = new QLabel(centerBox);
    centerForm->addRow(x0Label_, x0_);
    centerForm->addRow(y0Label_, y0_);
    paramsLayout->addWidget(centerBox);

    auto* lineBox = new QWidget(paramsGroup);
    auto* lineForm = new QFormLayout(lineBox);
    lineForm->setContentsMargins(0, 0, 0, 0);
    lineForm->setSpacing(8);
    x1_ = makeSpin(27, -50, 50);
    y1_ = makeSpin(18, -40, 40);
    x1Label_ = new QLabel(lineBox);
    y1Label_ = new QLabel(lineBox);
    lineForm->addRow(x1Label_, x1_);
    lineForm->addRow(y1Label_, y1_);
    lineFields_ = lineBox;
    paramsLayout->addWidget(lineBox);

    auto* rxBox = new QWidget(paramsGroup);
    auto* rxForm = new QFormLayout(rxBox);
    rxForm->setContentsMargins(0, 0, 0, 0);
    rxForm->setSpacing(8);
    radiusX_ = makeSpin(20, 0, 50);
    radiusXLabel_ = new QLabel(rxBox);
    rxForm->addRow(radiusXLabel_, radiusX_);
    radiusXField_ = rxBox;
    paramsLayout->addWidget(rxBox);

    auto* ryBox = new QWidget(paramsGroup);
    auto* ryForm = new QFormLayout(ryBox);
    ryForm->setContentsMargins(0, 0, 0, 0);
    ryForm->setSpacing(8);
    radiusY_ = makeSpin(12, 0, 40);
    radiusYLabel_ = new QLabel(ryBox);
    ryForm->addRow(radiusYLabel_, radiusY_);
    radiusYField_ = ryBox;
    paramsLayout->addWidget(ryBox);

    auto* angleBox = new QWidget(paramsGroup);
    auto* angleForm = new QFormLayout(angleBox);
    angleForm->setContentsMargins(0, 0, 0, 0);
    angleForm->setSpacing(8);
    startAngle_ = makeSpin(0, -360, 360);
    endAngle_ = makeSpin(180, -360, 360);
    angleForm->addRow(QStringLiteral("Начальный угол"), startAngle_);
    angleForm->addRow(QStringLiteral("Конечный угол"), endAngle_);
    angleFields_ = angleBox;
    paramsLayout->addWidget(angleBox);
    layout->addWidget(paramsGroup);

    auto* viewGroup = new QGroupBox(QStringLiteral("Масштаб"), panel);
    auto* viewLayout = new QVBoxLayout(viewGroup);
    auto* scaleForm = new QFormLayout();
    scale_ = makeSpin(12, 2, 40);
    scaleForm->addRow(QStringLiteral("Размер клетки, px"), scale_);
    viewLayout->addLayout(scaleForm);
    auto* resetZoom = new QPushButton(QStringLiteral("Сброс"), viewGroup);
    resetZoom->setObjectName(QStringLiteral("secondary"));
    viewLayout->addWidget(resetZoom);
    layout->addWidget(viewGroup);

    auto* draw = new QPushButton(QStringLiteral("Построить"), panel);
    auto* clear = new QPushButton(QStringLiteral("Очистить"), panel);
    clear->setObjectName(QStringLiteral("secondary"));
    layout->addWidget(draw);
    layout->addWidget(clear);
    layout->addStretch(1);

    connect(draw, &QPushButton::clicked, this, &MainWindow::Draw);
    connect(clear, &QPushButton::clicked, this, [this]() {
        grid_.Clear();
        raster_->SetGrid(&grid_);
        status_->setText(QStringLiteral("Поле очищено"));
    });
    connect(algorithm_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::UpdateParameterVisibility);
    auto invalidateExample = [this]() { if (!updatingExample_) steps_->clear(); };
    connect(x0_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(y0_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(x1_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(y1_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(radiusX_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(radiusY_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(startAngle_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(endAngle_, qOverload<int>(&QSpinBox::valueChanged), this, [invalidateExample](int) { invalidateExample(); });
    connect(primitive_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() { UpdateParameterVisibility(); });
    connect(scale_, qOverload<int>(&QSpinBox::valueChanged), raster_, &RasterWidget::SetScale);
    connect(raster_, &RasterWidget::ScaleChanged, scale_, &QSpinBox::setValue);
    connect(resetZoom, &QPushButton::clicked, raster_, &RasterWidget::ResetScale);
    UpdateParameterVisibility();
}

void MainWindow::UpdateParameterVisibility() {
    const int method = algorithm_->currentIndex();
    const QString previous = primitive_->currentText();
    primitive_->blockSignals(true);
    primitive_->clear();
    if (method == 2) {
        primitive_->addItems({QStringLiteral("Отрезок"), QStringLiteral("Окружность"), QStringLiteral("Эллипс")});
    } else if (method == 4) {
        primitive_->addItems({QStringLiteral("Эллипс"), QStringLiteral("Гипербола (правая ветвь)"),
                              QStringLiteral("Парабола"), QStringLiteral("Окружность"), QStringLiteral("Дуга эллипса")});
    }
    const int oldIndex = primitive_->findText(previous);
    if (oldIndex >= 0) primitive_->setCurrentIndex(oldIndex);
    primitive_->blockSignals(false);

    const bool lineMethod = method >= 0 && method <= 3;
    const bool bresenham = method == 2;
    const bool pitteway = method == 4;
    const int shape = primitive_->currentIndex();
    const bool line = lineMethod && (!bresenham || shape == 0);
    const bool circle = (bresenham && shape == 1) || (pitteway && shape == 3);
    const bool ellipse = (bresenham && shape == 2) || (pitteway && (shape == 0 || shape == 4));
    const bool hyperbola = pitteway && shape == 1;
    const bool parabola = pitteway && shape == 2;

    primitiveLabel_->setVisible(bresenham || pitteway);
    primitiveField_->setVisible(bresenham || pitteway);
    x0Label_->setText(line ? QStringLiteral("X₀") : QStringLiteral("Центр X"));
    y0Label_->setText(line ? QStringLiteral("Y₀") : QStringLiteral("Центр Y"));
    x1Label_->setText(QStringLiteral("X₁"));
    y1Label_->setText(QStringLiteral("Y₁"));
    radiusXLabel_->setText(circle ? QStringLiteral("Радиус R") : (parabola ? QStringLiteral("Параметр p") : QStringLiteral("Полуось a")));
    radiusYLabel_->setText(QStringLiteral("Полуось b"));

    lineFields_->setVisible(line);
    radiusXField_->setVisible(circle || ellipse || hyperbola || parabola);
    radiusYField_->setVisible(ellipse || hyperbola);
    angleFields_->setVisible(pitteway && shape == 4);
    ShowAlgorithmExample();
}

void MainWindow::ShowAlgorithmExample() {
    const int method = algorithm_->currentIndex();
    if (method < 0 || method == 3 || method == 4 || (method == 2 && primitive_->currentIndex() != 0)) {
        steps_->clear();
        Draw();
        return;
    }
    updatingExample_ = true;
    if (method <= 2) {
        x0_->setValue(2);
        y0_->setValue(2);
        x1_->setValue(8);
        y1_->setValue(5);
    } else {
        x0_->setValue(0);
        y0_->setValue(0);
        radiusX_->setValue(5);
    }
    updatingExample_ = false;
    Draw();

    QString text;
    if (method == 0) {
        text = QStringLiteral("Пошаговый алгоритм. Пример: (2, 2) → (8, 5)\n"
                              "Δx = 8 − 2 = 6; Δy = 5 − 2 = 3; N = max(|Δx|, |Δy|) = 6\n"
                              "Шаги: sx = 1; sy = 0,5\n"
                              "k   x       y       Пиксель\n");
        for (int k = 0; k <= 6; ++k) {
            const double x = 2.0 + k;
            const double y = 2.0 + 0.5 * k;
            text += QStringLiteral("%1   %2   %3   (%4, %5)\n")
                        .arg(k).arg(x, 0, 'f', 1).arg(y, 0, 'f', 1)
                        .arg(int(std::lround(x))).arg(int(std::lround(y)));
        }
    } else if (method == 1) {
        text = QStringLiteral("ЦДА. Пример: (2, 2) → (8, 5)\n"
                              "Δx = 6; Δy = 3; N = max(|6|, |3|) = 6\n"
                              "xш = Δx/N = 1; yш = Δy/N = 0,5\n"
                              "k   x       y       Округлённый пиксель\n");
        for (int k = 0; k <= 6; ++k) {
            const double x = 2.0 + k;
            const double y = 2.0 + 0.5 * k;
            text += QStringLiteral("%1   %2   %3   (%4, %5)\n")
                        .arg(k).arg(x, 0, 'f', 1).arg(y, 0, 'f', 1)
                        .arg(int(std::lround(x))).arg(int(std::lround(y)));
        }
    } else if (method == 2) {
        text = QStringLiteral("Брезенхем для отрезка. Пример: (2, 2) → (8, 5)\n"
                              "dx = 6; dy = −3; sx = 1; sy = 1; e₀ = dx + dy = 3\n"
                              "k   (x, y)   e   2e   Изменение\n");
        int x = 2, y = 2, dx = 6, dy = -3, sx = 1, sy = 1, error = 3, k = 0;
        for (;;) {
            const int oldX = x;
            const int oldY = y;
            const int oldError = error;
            const int doubled = 2 * error;
            QString change;
            if (x == 8 && y == 5) {
                text += QStringLiteral("%1   (%2, %3)   %4   —   конец\n").arg(k).arg(x).arg(y).arg(error);
                break;
            }
            if (doubled >= dy) { error += dy; x += sx; change += QStringLiteral("x; "); }
            if (doubled <= dx) { error += dx; y += sy; change += QStringLiteral("y"); }
            text += QStringLiteral("%1   (%2, %3)   %4   %5   %6\n")
                        .arg(k).arg(oldX).arg(oldY).arg(oldError).arg(doubled).arg(change);
            ++k;
        }
        text += QStringLiteral("Каждый шаг выбирает соседний пиксель по знаку удвоенной ошибки.");
    }
    steps_->setPlainText(text);
}

void MainWindow::Draw() {
    grid_.Clear();
    const int cx = grid_.Width() / 2;
    const int cy = grid_.Height() / 2;
    const int x0 = x0_->value() + cx;
    const int y0 = cy - y0_->value();
    const int x1 = x1_->value() + cx;
    const int y1 = cy - y1_->value();
    const int a = radiusX_->value();
    const int b = radiusY_->value();

    switch (algorithm_->currentIndex()) {
    case 0: RasterEngine::StepLine(grid_, x0, y0, x1, y1); break;
    case 1: RasterEngine::DDALine(grid_, x0, y0, x1, y1); break;
    case 2:
        switch (primitive_->currentIndex()) {
        case 0: RasterEngine::BresenhamLine(grid_, x0, y0, x1, y1); break;
        case 1: RasterEngine::BresenhamCircle(grid_, x0, y0, a); break;
        case 2: RasterEngine::BresenhamEllipse(grid_, x0, y0, a, b); break;
        }
        break;
    case 3: RasterEngine::WuLine(grid_, x0, y0, x1, y1); break;
    case 4: {
        RasterEngine::ConicType type = RasterEngine::ConicType::Ellipse;
        switch (primitive_->currentIndex()) {
        case 0: type = RasterEngine::ConicType::Ellipse; break;
        case 1: type = RasterEngine::ConicType::Hyperbola; break;
        case 2: type = RasterEngine::ConicType::Parabola; break;
        case 3: type = RasterEngine::ConicType::Circle; break;
        case 4: type = RasterEngine::ConicType::Arc; break;
        }
        RasterEngine::PittewayConic(grid_, x0, y0, a, b, type, startAngle_->value(), endAngle_->value());
        break;
    }
    default: break;
    }
    raster_->SetGrid(&grid_);
    raster_->SetScale(scale_->value());
    if (status_) status_->setText(QStringLiteral("Построено: %1 — %2").arg(algorithm_->currentText(), primitive_->currentText()));
}

void MainWindow::RunBenchmark() {
    using Series = BenchmarkWidget::Series;
    VirtualGrid target(2048, 2048);
    std::mt19937 rng(20261119);
    std::uniform_int_distribution<int> lineLengthDistribution(20, 350);
    std::uniform_int_distribution<int> radiusDistribution(2, 250);
    const QVector<int> lineCounts{1000, 5000, 10000, 25000, 50000, 75000, 100000};
    const QVector<int> lineLengths{50, 100, 200, 400, 600, 800, 1000};
    const QVector<int> circleCounts{1000, 5000, 10000, 25000, 50000};
    const QVector<int> circleRadii{10, 25, 50, 75, 100, 150, 200, 250};
    QVector<QPair<QPoint, QPoint>> lines;
    lines.reserve(100000);
    for (int i = 0; i < 100000; ++i) {
        const int length = lineLengthDistribution(rng);
        lines.push_back(randomOctantLine(rng, length, i % 8));
    }
    QVector<int> radii;
    radii.reserve(50000);
    for (int i = 0; i < 50000; ++i) radii.push_back(radiusDistribution(rng));

    auto measureTime = [&](const std::function<void()>& operation) {
        target.Clear();
        QElapsedTimer timer;
        timer.start();
        operation();
        return timer.nsecsElapsed() / 1000000.0;
    };

    const QVector<LineFunction> lineFunctions{
        &RasterEngine::StepLine,
        &RasterEngine::DDALine,
        &RasterEngine::BresenhamLine
    };
    const QVector<QString> lineNames{
        QStringLiteral("Пошаговый"), QStringLiteral("ЦДА"), QStringLiteral("Брезенхем")
    };
    const QVector<QColor> lineColors{
        QColor(QStringLiteral("#2878d0")), QColor(QStringLiteral("#16a085")), QColor(QStringLiteral("#d35400"))
    };
    QVector<Series> lineCountSeries;
    QVector<Series> lineLengthSeries;

    for (int method = 0; method < lineFunctions.size(); ++method) {
        Series countSeries;
        countSeries.name = lineNames[method];
        countSeries.color = lineColors[method];
        for (int count : lineCounts) {
            const double elapsed = measureTime([&]() {
                for (int i = 0; i < count; ++i) {
                    const auto& line = lines[i];
                    lineFunctions[method](target, line.first.x(), line.first.y(), line.second.x(), line.second.y());
                }
            });
            countSeries.x.push_back(count);
            countSeries.y.push_back(elapsed);
        }
        lineCountSeries.push_back(countSeries);

        Series lengthSeries;
        lengthSeries.name = lineNames[method];
        lengthSeries.color = lineColors[method];
        for (int length : lineLengths) {
            QVector<QPair<QPoint, QPoint>> samples;
            samples.reserve(5000);
            for (int i = 0; i < 5000; ++i) samples.push_back(randomOctantLine(rng, length, i % 8));
            const double elapsed = measureTime([&]() {
                for (const auto& line : samples)
                    lineFunctions[method](target, line.first.x(), line.first.y(), line.second.x(), line.second.y());
            });
            lengthSeries.x.push_back(length);
            lengthSeries.y.push_back(elapsed);
        }
        lineLengthSeries.push_back(lengthSeries);
    }

    Series circleCountSeries;
    circleCountSeries.name = QStringLiteral("Брезенхем — окружность");
    circleCountSeries.color = QColor(QStringLiteral("#8e44ad"));
    for (int count : circleCounts) {
        const double elapsed = measureTime([&]() {
            for (int i = 0; i < count; ++i)
                RasterEngine::BresenhamCircle(target, 1024, 1024, radii[i]);
        });
        circleCountSeries.x.push_back(count);
        circleCountSeries.y.push_back(elapsed);
    }

    Series circleRadiusSeries;
    circleRadiusSeries.name = QStringLiteral("Брезенхем — окружность");
    circleRadiusSeries.color = QColor(QStringLiteral("#8e44ad"));
    for (int radius : circleRadii) {
        const double elapsed = measureTime([&]() {
            for (int i = 0; i < 2000; ++i)
                RasterEngine::BresenhamCircle(target, 1024, 1024, radius);
        });
        circleRadiusSeries.x.push_back(radius);
        circleRadiusSeries.y.push_back(elapsed);
    }

    if (!benchmarkDialog_) {
        benchmarkDialog_ = new QDialog(this);
        benchmarkDialog_->setWindowTitle(QStringLiteral("Бенчмарк производительности"));
        benchmarkDialog_->resize(1500, 920);
        auto* dialogLayout = new QVBoxLayout(benchmarkDialog_);
        chart_ = new BenchmarkWidget(benchmarkDialog_);
        dialogLayout->addWidget(chart_);
    }
    chart_->SetData(lineCountSeries, lineLengthSeries, {circleCountSeries}, {circleRadiusSeries});
    benchmarkDialog_->show();
    benchmarkDialog_->raise();
    benchmarkDialog_->activateWindow();
    status_->setText(QStringLiteral("Бенчмарк завершён: 100 000 отрезков и 50 000 окружностей, медиана трёх запусков"));
}
