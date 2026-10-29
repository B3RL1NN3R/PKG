#include "MainWindow.h"
#include "processing/Filters.h"
#include "processing/Morphology.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QImage>
#include <QtConcurrent>
#include <QMessageBox>
#include <QGroupBox>
#include <QThread>
#include <QTimer>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Обработка цифровых изображений");
    resize(1100, 750);

    auto* central = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // === ЛЕВАЯ ЧАСТЬ: Панель управления ===
    auto* controlPanel = new QWidget(this);
    controlPanel->setMinimumWidth(320);
    controlPanel->setMaximumWidth(380);
    controlPanel->setStyleSheet("background: #f5f5f5; border-right: 1px solid #ddd;");

    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(20, 20, 20, 20);
    controlLayout->setSpacing(16);

    m_btnLoad = new QPushButton(" Загрузить изображение", this);
    m_btnLoad->setMinimumHeight(40);
    m_btnLoad->setEnabled(true);
    m_btnLoad->setStyleSheet(
        "QPushButton { background-color: #3498db; color: white; font-weight: bold; border-radius: 4px; } "
        "QPushButton:hover { background-color: #2980b9; } "
        "QPushButton:disabled { background-color: #a0c4e8; color: #ffffff; }"
        );
    controlLayout->addWidget(m_btnLoad);

    auto* groupParams = new QGroupBox("Параметры обработки", this);
    groupParams->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #bdc3c7; border-radius: 6px; "
        "margin-top: 12px; padding-top: 12px; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; "
        "color: #2c3e50; }"
        );
    auto* paramsLayout = new QVBoxLayout(groupParams);
    paramsLayout->setSpacing(12);

    paramsLayout->addWidget(new QLabel("Метод:", this));
    m_cbMethod = new QComboBox(this);
    m_cbMethod->setMinimumHeight(32);
    m_cbMethod->setEnabled(true);
    m_cbMethod->addItems({
        "Box Filter (сглаживание)",
        "Gaussian Filter (сглаживание)",
        "Erosion (эрозия)",
        "Dilation (дилатация)",
        "Morphological Gradient (градиент)",
        "Top-Hat (светлые детали)",
        "Black-Hat (тёмные детали)"
    });
    m_cbMethod->setStyleSheet(
        "QComboBox { "
        "    background-color: white; "
        "    color: #2c3e50; "
        "    border: 1px solid #bdc3c7; "
        "    border-radius: 4px; "
        "    padding: 4px 8px; "
        "    font-weight: bold; "
        "} "
        "QComboBox:hover { border-color: #3498db; } "
        "QComboBox:focus { border-color: #2980b9; } "
        "QComboBox:disabled { background-color: #ecf0f1; color: #7f8c8d; } "
        "QComboBox::drop-down { border: none; background: transparent; } "
        "QComboBox QAbstractItemView { "
        "    background-color: white; color: #2c3e50; "
        "    selection-background-color: #3498db; selection-color: white; "
        "    border: 1px solid #bdc3c7; outline: none; "
        "}"
        );
    paramsLayout->addWidget(m_cbMethod);

    paramsLayout->addWidget(new QLabel("Режим обработки краёв:", this));
    m_cbEdge = new QComboBox(this);
    m_cbEdge->setMinimumHeight(32);
    m_cbEdge->setEnabled(true);
    m_cbEdge->addItems({
        "Clamp (дублирование граничных пикселей)",
        "Reflect (зеркальное отражение)"
    });
    m_cbEdge->setStyleSheet(
        "QComboBox { "
        "    background-color: white; "
        "    color: #2c3e50; "
        "    border: 1px solid #bdc3c7; "
        "    border-radius: 4px; "
        "    padding: 4px 8px; "
        "    font-weight: bold; "
        "} "
        "QComboBox:hover { border-color: #3498db; } "
        "QComboBox:focus { border-color: #2980b9; } "
        "QComboBox:disabled { background-color: #ecf0f1; color: #7f8c8d; } "
        "QComboBox::drop-down { border: none; background: transparent; } "
        "QComboBox QAbstractItemView { "
        "    background-color: white; color: #2c3e50; "
        "    selection-background-color: #3498db; selection-color: white; "
        "    border: 1px solid #bdc3c7; outline: none; "
        "}"
        );
    paramsLayout->addWidget(m_cbEdge);

    m_lblEdgeDesc = new QLabel(this);
    m_lblEdgeDesc->setStyleSheet(
        "QLabel { color: #555; font-size: 11px; background: #ecf0f1; "
        "padding: 8px; border-radius: 4px; border-left: 3px solid #3498db; }"
        );
    m_lblEdgeDesc->setWordWrap(true);
    m_lblEdgeDesc->setMinimumHeight(60);
    paramsLayout->addWidget(m_lblEdgeDesc);

    paramsLayout->addWidget(new QLabel("Радиус ядра / структурирующего элемента:", this));
    auto* radiusRow = new QHBoxLayout();
    m_sliderRadius = new QSlider(Qt::Horizontal, this);
    m_sliderRadius->setRange(1, 15);
    m_sliderRadius->setValue(2);
    m_sliderRadius->setEnabled(true);
    m_lblRadiusValue = new QLabel("2", this);
    m_lblRadiusValue->setMinimumWidth(30);
    m_lblRadiusValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblRadiusValue->setStyleSheet("font-weight: bold; color: #2c3e50;");
    radiusRow->addWidget(m_sliderRadius);
    radiusRow->addWidget(m_lblRadiusValue);
    paramsLayout->addLayout(radiusRow);

    controlLayout->addWidget(groupParams);

    m_btnApply = new QPushButton("▶ Применить", this);
    m_btnApply->setMinimumHeight(44);
    m_btnApply->setEnabled(false);
    m_btnApply->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; font-weight: bold; font-size: 14px; border-radius: 4px; } "
        "QPushButton:hover { background-color: #229954; } "
        "QPushButton:pressed { background-color: #1e8449; } "
        "QPushButton:disabled { background-color: #95a5a6; color: #7f8c8d; }"
        );
    controlLayout->addWidget(m_btnApply);

    m_btnSave = new QPushButton("💾 Сохранить результат", this);
    m_btnSave->setMinimumHeight(36);
    m_btnSave->setEnabled(false);
    m_btnSave->setStyleSheet(
        "QPushButton { background-color: #9b59b6; color: white; font-weight: bold; border-radius: 4px; } "
        "QPushButton:hover { background-color: #8e44ad; } "
        "QPushButton:pressed { background-color: #7d3c98; } "
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }"
        );
    controlLayout->addWidget(m_btnSave);

    controlLayout->addStretch();
    rootLayout->addWidget(controlPanel, 0);

    // === ПРАВАЯ ЧАСТЬ: Изображения ===
    auto* imagesWidget = new QWidget(this);
    auto* imagesLayout = new QVBoxLayout(imagesWidget);
    imagesLayout->setContentsMargins(20, 20, 20, 20);
    imagesLayout->setSpacing(16);

    auto* lblTitleOrig = new QLabel("Оригинальное изображение", this);
    lblTitleOrig->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50;");

    m_lblOriginal = new QLabel("Изображение не загружено", this);
    m_lblOriginal->setAlignment(Qt::AlignCenter);
    m_lblOriginal->setStyleSheet(
        "QLabel { background: #ecf0f1; color: #7f8c8d; border: 2px dashed #bdc3c7; border-radius: 4px; }");
    m_lblOriginal->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_lblOriginal->setScaledContents(false);

    auto* lblTitleRes = new QLabel("Обработанное изображение", this);
    lblTitleRes->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50;");

    m_lblResult = new QLabel("Настройте параметры и нажмите «Применить»", this);
    m_lblResult->setAlignment(Qt::AlignCenter);
    m_lblResult->setStyleSheet(
        "QLabel { background: #ecf0f1; color: #7f8c8d; border: 2px dashed #bdc3c7; border-radius: 4px; }");
    m_lblResult->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_lblResult->setScaledContents(false);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 1px solid #bdc3c7; border-radius: 4px; text-align: center; } "
        "QProgressBar::chunk { background-color: #3498db; }"
        );

    m_lblStatus = new QLabel("Готов к работе", this);
    m_lblStatus->setStyleSheet("color: #7f8c8d; font-size: 12px;");

    imagesLayout->addWidget(lblTitleOrig);
    imagesLayout->addWidget(m_lblOriginal, 1);
    imagesLayout->addWidget(lblTitleRes);
    imagesLayout->addWidget(m_lblResult, 1);
    imagesLayout->addWidget(m_progressBar);
    imagesLayout->addWidget(m_lblStatus);

    rootLayout->addWidget(imagesWidget, 1);

    setCentralWidget(central);

    connect(m_btnLoad, &QPushButton::clicked, this, &MainWindow::onLoadImage);
    connect(m_btnApply, &QPushButton::clicked, this, &MainWindow::onApply);
    connect(m_btnSave, &QPushButton::clicked, this, &MainWindow::onSave);

    connect(m_cbEdge, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onEdgeModeChanged);
    connect(m_sliderRadius, &QSlider::valueChanged, this, &MainWindow::onRadiusChanged);
    connect(m_cbMethod, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onMethodChanged);
    connect(&m_watcher, &QFutureWatcher<ImageBuffer>::finished,
            this, &MainWindow::onProcessingFinished);

    onEdgeModeChanged(0);
    onMethodChanged(0);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    rescaleImages();
}

void MainWindow::rescaleImages() {
    if (!m_originalPixmap.isNull()) {
        m_lblOriginal->setPixmap(
            m_originalPixmap.scaled(m_lblOriginal->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation));
    }
    if (!m_resultPixmap.isNull()) {
        m_lblResult->setPixmap(
            m_resultPixmap.scaled(m_lblResult->size(),
                                  Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation));
    }
}

void MainWindow::showWarning(const QString& message) {
    QMessageBox::warning(this, "Предупреждение", message);
}

int MainWindow::getOptimalRadius(int method) const {
    switch (method) {
    case 0: return 3;  // Box Filter
    case 1: return 3;  // Gaussian Filter
    case 2: return 2;  // Erosion
    case 3: return 2;  // Dilation
    case 4: return 1;  // Morphological Gradient
    case 5: return 2;  // Top-Hat
    case 6: return 2;  // Black-Hat
    default: return 2;
    }
}

int MainWindow::getMaxRecommendedRadius(int method) const {
    switch (method) {
    case 0: return 7;  // Box Filter
    case 1: return 8;  // Gaussian Filter
    case 2: return 4;  // Erosion
    case 3: return 4;  // Dilation
    case 4: return 3;  // Morphological Gradient
    case 5: return 4;  // Top-Hat
    case 6: return 4;  // Black-Hat
    default: return 5;
    }
}

void MainWindow::onMethodChanged(int index) {
    int optimal = getOptimalRadius(index);
    int maxRec = getMaxRecommendedRadius(index);

    m_sliderRadius->setValue(optimal);
    m_sliderRadius->setMaximum(maxRec);

    QString methodNames[] = {
        "Box Filter", "Gaussian Filter", "Erosion", "Dilation",
        "Morphological Gradient", "Top-Hat", "Black-Hat"
    };

    m_lblStatus->setText(QString("Метод: %1 | Оптимальный радиус: %2 | Макс. рекомендуемый: %3")
                             .arg(methodNames[index]).arg(optimal).arg(maxRec));
}

bool MainWindow::validateParameters() {
    int radius = m_sliderRadius->value();

    if (radius < 1 || radius > 15) {
        showWarning("Радиус должен быть в диапазоне от 1 до 15");
        return false;
    }

    return true;
}

void MainWindow::onEdgeModeChanged(int index) {
    if (index == 0) {
        m_lblEdgeDesc->setText(
            "<b>Clamp:</b> граничные пиксели дублируются. Может создавать артефакты-полосы "
            "на краях при сильной фильтрации.");
    } else {
        m_lblEdgeDesc->setText(
            "<b>Reflect:</b> изображение зеркально отражается за границей. Даёт более "
            "естественный результат при свёртке, рекомендуется для НЧ-фильтров.");
    }
}

void MainWindow::onRadiusChanged(int value) {
    m_lblRadiusValue->setText(QString::number(value));
}

void MainWindow::onLoadImage() {
    if (m_watcher.isRunning()) {
        showWarning("Дождитесь завершения текущей обработки");
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, "Выберите изображение", QString(),
                                                "Изображения (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) {
        showWarning("Не удалось загрузить изображение. Проверьте формат файла.");
        return;
    }

    m_source = std::make_unique<ImageBuffer>(img);
    m_originalPixmap = QPixmap::fromImage(img);

    m_lblOriginal->setText("");
    m_lblOriginal->setStyleSheet(
        "QLabel { background: transparent; border: 1px solid #bdc3c7; border-radius: 4px; }");

    m_btnApply->setEnabled(true);

    m_btnSave->setEnabled(false);
    m_resultPixmap = QPixmap();
    m_lblResult->setText("Настройте параметры и нажмите «Применить»");
    m_lblResult->setStyleSheet(
        "QLabel { background: #ecf0f1; color: #7f8c8d; border: 2px dashed #bdc3c7; border-radius: 4px; }");

    rescaleImages();
}

void MainWindow::onApply() {
    if (!m_source) {
        showWarning("Сначала загрузите изображение");
        return;
    }
    if (!validateParameters()) return;
    if (m_watcher.isRunning()) {
        showWarning("Обработка уже выполняется. Дождитесь завершения.");
        return;
    }

    m_btnLoad->setEnabled(false);
    m_cbMethod->setEnabled(false);
    m_cbEdge->setEnabled(false);
    m_sliderRadius->setEnabled(false);
    m_btnApply->setEnabled(false);
    m_btnApply->setText("⏳ Обработка...");

    m_lblResult->setText("Обработка...");
    m_lblResult->setPixmap(QPixmap());

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_lblStatus->setText("Начало обработки...");

    runProcessing();
}

void MainWindow::runProcessing() {
    ImageBuffer srcCopy(*m_source);
    int method = m_cbMethod->currentIndex();
    int edge = m_cbEdge->currentIndex();
    int radius = m_sliderRadius->value();
    EdgeMode mode = (edge == 0) ? EdgeMode::Clamp : EdgeMode::Reflect;

    auto future = QtConcurrent::run([=]() -> ImageBuffer {
        switch (method) {
        case 0: return Filters::boxFilter(srcCopy, radius, mode);
        case 1: return Filters::gaussianFilter(srcCopy, radius, mode);
        case 2: return Morphology::apply(srcCopy, MorphOp::Erosion, radius, mode);
        case 3: return Morphology::apply(srcCopy, MorphOp::Dilation, radius, mode);
        case 4: return Morphology::apply(srcCopy, MorphOp::Gradient, radius, mode);
        case 5: return Morphology::apply(srcCopy, MorphOp::TopHat, radius, mode);
        case 6: return Morphology::apply(srcCopy, MorphOp::BlackHat, radius, mode);
        }
        return srcCopy;
    });
    m_watcher.setFuture(future);
}

void MainWindow::onProcessingFinished() {
    m_btnLoad->setEnabled(true);
    m_cbMethod->setEnabled(true);
    m_cbEdge->setEnabled(true);
    m_sliderRadius->setEnabled(true);
    m_btnApply->setEnabled(true);
    m_btnApply->setText("▶ Применить");

    m_progressBar->setValue(100);
    m_lblStatus->setText("Обработка завершена");

    ImageBuffer result = m_watcher.result();
    m_resultPixmap = QPixmap::fromImage(result.toQImage());

    m_lblResult->setText("");
    m_lblResult->setStyleSheet(
        "QLabel { background: transparent; border: 1px solid #bdc3c7; border-radius: 4px; }");
    m_btnSave->setEnabled(true);

    rescaleImages();

    QTimer::singleShot(2000, [this]() {
        m_progressBar->setVisible(false);
        m_lblStatus->setText("Готов к работе");
    });
}

void MainWindow::onSave() {
    if (m_resultPixmap.isNull()) return;

    QString path = QFileDialog::getSaveFileName(this, "Сохранить результат", QString(),
                                                "PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)");
    if (path.isEmpty()) return;

    if (!m_resultPixmap.toImage().save(path)) {
        showWarning("Не удалось сохранить изображение. Проверьте путь и права доступа.");
    }
}