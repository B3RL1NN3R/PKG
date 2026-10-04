#include "MainWindow.h"

#include "parser/ImageParser.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

namespace
{
DiscoveryResult discoverFiles(const QString& folder, bool recursive,
                              const std::shared_ptr<std::atomic_bool>& cancelToken)
{
    DiscoveryResult result;
    static const QSet<QString> supported = {
        "jpg", "jpeg", "gif", "tif", "tiff", "bmp", "png", "pcx"
    };

    const QDirIterator::IteratorFlag flag = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;

    QDirIterator iterator(folder, QDir::Files | QDir::Readable | QDir::NoSymLinks, flag);
    while (iterator.hasNext())
    {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed))
        {
            result.cancelled = true;
            return result;
        }

        const QString path = iterator.next();
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (!supported.contains(suffix))
            continue;

        result.files.push_back(path);
        if (result.files.size() > 100000)
        {
            result.error = "В папке найдено больше 100000 поддерживаемых файлов.";
            result.files.clear();
            return result;
        }
    }

    return result;
}

QString selectedSummary(const ImageMetadata& metadata)
{
    QStringList lines;
    lines << QString("Формат: %1").arg(metadata.format);
    if (metadata.width > 0 && metadata.height > 0)
        lines << QString("Размер: %1 x %2 px").arg(metadata.width).arg(metadata.height);
    if (metadata.dpiX > 0.0 && metadata.dpiY > 0.0)
    {
        QString resolution = QString("Разрешение: %1 x %2 dpi")
            .arg(metadata.dpiX, 0, 'f', 2).arg(metadata.dpiY, 0, 'f', 2);
        if (metadata.windowsDefaultDpi)
            resolution += " (Windows по умолчанию)";
        lines << resolution;
    }
    else
        lines << "Разрешение: -";
    lines << QString("Глубина цвета: %1")
        .arg(metadata.colorDepthText.isEmpty() ? QString("-") : metadata.colorDepthText);
    lines << QString("Сжатие: %1").arg(metadata.compression.isEmpty() ? QString("-") : metadata.compression);
    lines << QString("Статус: %1").arg(metadata.statusText);
    if (!metadata.details.isEmpty())
        lines << QString("Детали: %1").arg(metadata.details);
    return lines.join("\n");
}
}

MainWindow::MainWindow()
{
    setWindowTitle("Image Metadata Reader");
    resize(1420, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(12);

    auto* title = new QLabel("Чтение информации из графических файлов", central);
    title->setObjectName("titleLabel");
    auto* subtitle = new QLabel(
        "Ручной разбор JPEG, GIF, TIFF, BMP, PNG и PCX без библиотек метаданных", central);
    subtitle->setObjectName("subtitleLabel");
    rootLayout->addWidget(title);
    rootLayout->addWidget(subtitle);

    auto* folderRow = new QHBoxLayout();
    folderEdit_ = new QLineEdit(central);
    folderEdit_->setReadOnly(true);
    folderEdit_->setPlaceholderText("Выберите папку с изображениями");
    chooseButton_ = new QPushButton("Выбрать папку", central);
    scanButton_ = new QPushButton("Сканировать", central);
    scanButton_->setProperty("primary", true);
    cancelButton_ = new QPushButton("Отмена", central);
    cancelButton_->setEnabled(false);
    recursiveCheck_ = new QCheckBox("Включать подпапки", central);

    folderRow->addWidget(folderEdit_, 1);
    folderRow->addWidget(chooseButton_);
    folderRow->addWidget(scanButton_);
    folderRow->addWidget(cancelButton_);
    folderRow->addWidget(recursiveCheck_);
    rootLayout->addLayout(folderRow);

    auto* progressRow = new QHBoxLayout();
    progressBar_ = new QProgressBar(central);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    statusLabel_ = new QLabel("Готово к работе", central);
    statusLabel_->setMinimumWidth(280);
    progressRow->addWidget(progressBar_, 1);
    progressRow->addWidget(statusLabel_);
    rootLayout->addLayout(progressRow);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    tableView_ = new QTableView(splitter);
    tableView_->setModel(&model_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setAlternatingRowColors(true);
    tableView_->setSortingEnabled(false);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->horizontalHeader()->setSectionsMovable(true);
    tableView_->setColumnWidth(0, 220);
    tableView_->setColumnWidth(1, 80);
    tableView_->setColumnWidth(2, 120);
    tableView_->setColumnWidth(3, 140);
    tableView_->setColumnWidth(4, 170);
    tableView_->setColumnWidth(5, 190);
    tableView_->setColumnWidth(6, 140);

    auto* detailsPanel = new QFrame(splitter);
    detailsPanel->setObjectName("detailsPanel");
    detailsPanel->setMinimumWidth(330);
    auto* detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(16, 16, 16, 16);
    detailsLayout->setSpacing(10);

    selectedFileLabel_ = new QLabel("Файл не выбран", detailsPanel);
    selectedFileLabel_->setObjectName("selectedFile");
    selectedFileLabel_->setWordWrap(true);
    previewLabel_ = new QLabel(detailsPanel);
    previewLabel_->setObjectName("preview");
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(300, 260);
    previewLabel_->setText("Предпросмотр");
    selectedInfoLabel_ = new QLabel(detailsPanel);
    selectedInfoLabel_->setWordWrap(true);
    selectedInfoLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selectedInfoLabel_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    detailsLayout->addWidget(selectedFileLabel_);
    detailsLayout->addWidget(previewLabel_);
    detailsLayout->addWidget(selectedInfoLabel_, 1);

    splitter->addWidget(tableView_);
    splitter->addWidget(detailsPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({1050, 350});
    rootLayout->addWidget(splitter, 1);

    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow {
            background: #f5f7fa;
        }
        QWidget {
            color: #20242a;
            font-family: "Segoe UI", "Arial";
            font-size: 13px;
        }
        QLabel#titleLabel {
            color: #171a1f;
            font-size: 24px;
            font-weight: 600;
        }
        QLabel#subtitleLabel {
            color: #6f7680;
        }
        QLineEdit {
            min-height: 30px;
            padding: 3px 9px;
            background: white;
            border: 1px solid #cfd5dc;
            border-radius: 6px;
        }
        QPushButton {
            min-height: 30px;
            padding: 3px 12px;
            background: white;
            border: 1px solid #cfd5dc;
            border-radius: 6px;
        }
        QPushButton:hover {
            background: #f0f3f7;
        }
        QPushButton[primary="true"] {
            color: white;
            background: #2563eb;
            border-color: #2563eb;
        }
        QPushButton[primary="true"]:hover {
            background: #1d4ed8;
        }
        QPushButton:disabled {
            color: #9aa1aa;
            background: #eef1f4;
        }
        QProgressBar {
            min-height: 20px;
            border: 1px solid #d5dae0;
            border-radius: 5px;
            background: white;
            text-align: center;
        }
        QProgressBar::chunk {
            background: #2563eb;
            border-radius: 4px;
        }
        QTableView {
            background: white;
            alternate-background-color: #f8fafc;
            border: 1px solid #dfe3e8;
            border-radius: 7px;
            gridline-color: #edf0f2;
            selection-background-color: #dbeafe;
            selection-color: #111827;
        }
        QHeaderView::section {
            background: #eef2f7;
            border: none;
            border-right: 1px solid #dfe3e8;
            border-bottom: 1px solid #dfe3e8;
            padding: 7px;
            font-weight: 600;
        }
        QFrame#detailsPanel {
            background: white;
            border: 1px solid #dfe3e8;
            border-radius: 7px;
        }
        QLabel#selectedFile {
            font-size: 16px;
            font-weight: 600;
        }
        QLabel#preview {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            color: #8a929c;
        }
    )");

    connect(chooseButton_, &QPushButton::clicked, this, [this] { chooseFolder(); });
    connect(scanButton_, &QPushButton::clicked, this, [this] { startScan(); });
    connect(cancelButton_, &QPushButton::clicked, this, [this] { cancelScan(); });

    connect(&discoveryWatcher_, &QFutureWatcher<DiscoveryResult>::finished, this, [this] {
        if (discoveryWatcher_.isCanceled())
        {
            finishScan();
            statusLabel_->setText("Сканирование отменено");
            return;
        }
        const DiscoveryResult result = discoveryWatcher_.result();
        if (result.cancelled || (cancelToken_ && cancelToken_->load(std::memory_order_relaxed)))
        {
            finishScan();
            statusLabel_->setText("Сканирование отменено");
            return;
        }
        if (!result.error.isEmpty())
        {
            finishScan();
            statusLabel_->setText(result.error);
            return;
        }
        startParsing(result.files);
    });

    connect(&parseWatcher_, &QFutureWatcher<ImageMetadata>::progressRangeChanged,
            progressBar_, &QProgressBar::setRange);
    connect(&parseWatcher_, &QFutureWatcher<ImageMetadata>::progressValueChanged,
            progressBar_, &QProgressBar::setValue);
    connect(&parseWatcher_, &QFutureWatcher<ImageMetadata>::resultsReadyAt,
            this, [this](int beginIndex, int endIndex) {
        QVector<ImageMetadata> batch;
        batch.reserve(endIndex - beginIndex);
        for (int i = beginIndex; i < endIndex; ++i)
            batch.push_back(parseWatcher_.resultAt(i));
        model_.appendBatch(std::move(batch));
        statusLabel_->setText(QString("Обработано %1 из %2")
            .arg(parseWatcher_.progressValue()).arg(parseWatcher_.progressMaximum()));
    });
    connect(&parseWatcher_, &QFutureWatcher<ImageMetadata>::finished, this, [this] {
        const bool cancelled = parseWatcher_.isCanceled() ||
            (cancelToken_ && cancelToken_->load(std::memory_order_relaxed));
        const qint64 milliseconds = elapsed_.elapsed();
        const int rows = model_.rowCount();
        const int corrupted = model_.corruptedCount();
        const int warnings = model_.warningCount();
        finishScan();
        if (cancelled)
        {
            statusLabel_->setText(QString("Отменено. Обработано: %1").arg(rows));
        }
        else
        {
            statusLabel_->setText(QString("Готово: %1 файлов, повреждено %2, предупреждений %3, %4 с")
                .arg(rows).arg(corrupted).arg(warnings).arg(milliseconds / 1000.0, 0, 'f', 2));
        }
    });

    connect(tableView_->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) { showMetadata(current); });
}

MainWindow::~MainWindow()
{
    if (cancelToken_)
        cancelToken_->store(true, std::memory_order_relaxed);
    discoveryWatcher_.cancel();
    parseWatcher_.cancel();
    discoveryWatcher_.waitForFinished();
    parseWatcher_.waitForFinished();
}

void MainWindow::chooseFolder()
{
    const QString initial = folderEdit_->text().isEmpty()
        ? QDir::homePath()
        : folderEdit_->text();
    const QString folder = QFileDialog::getExistingDirectory(this, "Выберите папку", initial);
    if (!folder.isEmpty())
        folderEdit_->setText(QDir::toNativeSeparators(folder));
}

void MainWindow::startScan()
{
    if (busy_)
        return;
    if (folderEdit_->text().isEmpty())
    {
        chooseFolder();
        if (folderEdit_->text().isEmpty())
            return;
    }

    model_.clear();
    clearPreview();
    cancelToken_ = std::make_shared<std::atomic_bool>(false);
    elapsed_.restart();
    setBusy(true);
    startDiscovery();
}

void MainWindow::cancelScan()
{
    if (!busy_)
        return;
    if (cancelToken_)
        cancelToken_->store(true, std::memory_order_relaxed);
    discoveryWatcher_.cancel();
    parseWatcher_.cancel();
    cancelButton_->setEnabled(false);
    statusLabel_->setText("Отмена...");
}

void MainWindow::startDiscovery()
{
    progressBar_->setRange(0, 0);
    statusLabel_->setText("Поиск поддерживаемых файлов...");
    const QString folder = QDir::fromNativeSeparators(folderEdit_->text());
    const bool recursive = recursiveCheck_->isChecked();
    const auto token = cancelToken_;
    discoveryWatcher_.setFuture(QtConcurrent::run([folder, recursive, token] {
        return discoverFiles(folder, recursive, token);
    }));
}

void MainWindow::startParsing(const QStringList& files)
{
    if (files.isEmpty())
    {
        finishScan();
        progressBar_->setRange(0, 100);
        progressBar_->setValue(0);
        statusLabel_->setText("Поддерживаемые файлы не найдены");
        return;
    }

    progressBar_->setRange(0, files.size());
    progressBar_->setValue(0);
    statusLabel_->setText(QString("Найдено файлов: %1. Чтение метаданных...").arg(files.size()));
    parseWatcher_.setPendingResultsLimit(256);
    parseWatcher_.setFuture(QtConcurrent::mapped(files, [](const QString& path) {
        return ImageParser::parseFile(path);
    }));
}

void MainWindow::finishScan()
{
    setBusy(false);
    if (progressBar_->maximum() == 0)
    {
        progressBar_->setRange(0, 100);
        progressBar_->setValue(0);
    }
    else if (!parseWatcher_.isCanceled())
    {
        progressBar_->setValue(progressBar_->maximum());
    }
}

void MainWindow::setBusy(bool busy)
{
    busy_ = busy;
    chooseButton_->setEnabled(!busy);
    scanButton_->setEnabled(!busy);
    recursiveCheck_->setEnabled(!busy);
    cancelButton_->setEnabled(busy);
}

void MainWindow::showMetadata(const QModelIndex& current)
{
    const ImageMetadata* metadata = model_.metadataAt(current.row());
    if (!metadata)
    {
        clearPreview();
        return;
    }

    selectedFileLabel_->setText(metadata->fileName);
    selectedInfoLabel_->setText(selectedSummary(*metadata));

    if (metadata->state == MetadataState::Corrupted || metadata->width <= 0 || metadata->height <= 0)
    {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText("Предпросмотр недоступен");
        return;
    }

    QPixmap pixmap(metadata->filePath);
    if (pixmap.isNull())
    {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText("Qt не может отобразить этот формат");
        return;
    }

    previewLabel_->setText(QString());
    previewLabel_->setPixmap(pixmap.scaled(300, 260, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::clearPreview()
{
    selectedFileLabel_->setText("Файл не выбран");
    selectedInfoLabel_->clear();
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText("Предпросмотр");
}
