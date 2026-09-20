#include "MainWindow.h"

#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
void drawChevron(QPainter& painter, const QRect& area, bool pointsUp)
{
    const QPoint center = area.center();
    const int direction = pointsUp ? -1 : 1;

    painter.drawLine(center.x() - 4, center.y() - 2 * direction,
                     center.x(), center.y() + 2 * direction);
    painter.drawLine(center.x(), center.y() + 2 * direction,
                     center.x() + 4, center.y() - 2 * direction);
}

void drawSpinBoxArrows(QWidget* widget, const QStyleOptionSpinBox& option)
{
    const QRect upButton = widget->style()->subControlRect(
        QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxUp, widget);
    const QRect downButton = widget->style()->subControlRect(
        QStyle::CC_SpinBox, &option, QStyle::SC_SpinBoxDown, widget);

    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(QColor(66, 74, 84));
    pen.setWidthF(1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    drawChevron(painter, upButton, true);
    drawChevron(painter, downButton, false);
}

class ArrowSpinBox : public QSpinBox
{
public:
    ArrowSpinBox()
    {
        lineEdit()->setReadOnly(true);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QSpinBox::paintEvent(event);
        QStyleOptionSpinBox option;
        initStyleOption(&option);
        drawSpinBoxArrows(this, option);
    }
};

class ArrowDoubleSpinBox : public QDoubleSpinBox
{
public:
    ArrowDoubleSpinBox()
    {
        lineEdit()->setReadOnly(true);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QDoubleSpinBox::paintEvent(event);
        QStyleOptionSpinBox option;
        initStyleOption(&option);
        drawSpinBoxArrows(this, option);
    }
};

class JumpSlider : public QSlider
{
public:
    explicit JumpSlider(Qt::Orientation orientation, QWidget* parent = nullptr)
        : QSlider(orientation, parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            QStyleOptionSlider option;
            initStyleOption(&option);

            const QRect groove = style()->subControlRect(
                QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
            const QRect handle = style()->subControlRect(
                QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const QPoint click = event->position().toPoint();
#else
            const QPoint click = event->pos();
#endif

            const int sliderLength = orientation() == Qt::Horizontal
                ? handle.width()
                : handle.height();
            const int sliderMinimum = orientation() == Qt::Horizontal
                ? groove.left()
                : groove.top();
            const int sliderMaximum = orientation() == Qt::Horizontal
                ? groove.right() - sliderLength + 1
                : groove.bottom() - sliderLength + 1;
            const int clickPosition = orientation() == Qt::Horizontal
                ? click.x()
                : click.y();
            const int handlePosition = std::clamp(
                clickPosition - sliderLength / 2, sliderMinimum, sliderMaximum);

            setValue(QStyle::sliderValueFromPosition(
                minimum(), maximum(), handlePosition - sliderMinimum,
                sliderMaximum - sliderMinimum, option.upsideDown));
        }

        // После перехода ручка уже находится под курсором, поэтому стандартная
        // обработка Qt сохраняет возможность сразу продолжить перетаскивание.
        QSlider::mousePressEvent(event);
    }
};

double parseNumber(QString text, bool& ok)
{
    text = text.trimmed();
    text.replace(',', '.');
    return text.toDouble(&ok);
}

}

MainWindow::MainWindow()
    : controller_([this](const ColorUpdate& update) { showUpdate(update); })
{
    setWindowTitle("ColorConverter");
    resize(1180, 680);
    setStyleSheet(R"(
        QMainWindow, QDialog {
            background: #f6f7f9;
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
            color: #717780;
        }
        QLabel#conversionNotice, QLabel#inputError {
            color: #805400;
        }
        QGroupBox {
            background: #ffffff;
            border: 1px solid #dfe3e8;
            border-radius: 8px;
            margin-top: 14px;
            padding: 16px 12px 12px 12px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 5px;
            color: #30353b;
        }
        QPushButton {
            min-height: 20px;
            padding: 5px 10px;
            background: #ffffff;
            border: 1px solid #cfd5dc;
            border-radius: 5px;
        }
        QPushButton:hover {
            background: #f1f4f7;
            border-color: #aeb7c2;
        }
        QPushButton:pressed {
            background: #e7ebef;
        }
        QPushButton[primary="true"] {
            color: #ffffff;
            background: #2563eb;
            border-color: #2563eb;
        }
        QPushButton[primary="true"]:hover {
            background: #1d4ed8;
        }
        QSpinBox, QDoubleSpinBox, QLineEdit {
            min-height: 24px;
            padding: 3px 25px 3px 7px;
            background: #ffffff;
            border: 1px solid #cfd5dc;
            border-radius: 5px;
            selection-background-color: #2563eb;
        }
        QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
            border-color: #2563eb;
        }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 21px;
            background: #f4f6f8;
            border-left: 1px solid #d5dae0;
            border-bottom: 1px solid #dfe3e8;
            border-top-right-radius: 5px;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 21px;
            background: #f4f6f8;
            border-left: 1px solid #d5dae0;
            border-top: 1px solid #dfe3e8;
            border-bottom-right-radius: 5px;
        }
        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
            background: #e5e9ee;
        }
        QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,
        QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {
            background: #d9dee5;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: none;
            width: 0;
            height: 0;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #dde2e8;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #2563eb;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            background: #ffffff;
            border: 2px solid #2563eb;
            border-radius: 7px;
        }
    )");

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(22, 18, 22, 14);
    root->setSpacing(12);

    auto* title = new QLabel("ColorConverter", central);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto* subtitle = new QLabel("CMYK ↔ RGB ↔ HLS", central);
    subtitle->setObjectName("subtitleLabel");
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(subtitle);

    auto* panels = new QHBoxLayout;
    panels->setSpacing(12);
    panels->addWidget(createCmykPanel());
    panels->addWidget(createRgbPanel());
    panels->addWidget(createHlsPanel());
    root->addLayout(panels, 1);

    auto* previewTitle = new QLabel("Итоговый цвет", central);
    previewTitle->setAlignment(Qt::AlignCenter);
    root->addWidget(previewTitle);

    preview_ = new QLabel(central);
    preview_->setObjectName("colorPreview");
    preview_->setMinimumHeight(90);
    preview_->setFrameShape(QFrame::StyledPanel);
    root->addWidget(preview_);

    notice_ = new QLabel(central);
    notice_->setObjectName("conversionNotice");
    notice_->setWordWrap(true);
    notice_->setAlignment(Qt::AlignCenter);
    notice_->setMinimumHeight(36);
    notice_->setAccessibleName("Предупреждение о преобразовании цвета");
    root->addWidget(notice_);

    setCentralWidget(central);

    connectRgbControl(red_);
    connectRgbControl(green_);
    connectRgbControl(blue_);
    connectCmykControl(cyan_);
    connectCmykControl(magenta_);
    connectCmykControl(yellow_);
    connectCmykControl(key_);
    connectHlsControl(hue_);
    connectHlsControl(lightness_);
    connectHlsControl(saturation_);

    showUpdate({controller_.state(), ""});
}

QWidget* MainWindow::createIntRow(const QString& name, int maximum, IntControl& control)
{
    auto* row = new QWidget;
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout;
    auto* label = new QLabel(name);
    control.input = new ArrowSpinBox;
    control.input->setRange(0, maximum);
    control.input->setSingleStep(1);
    control.input->setMinimumWidth(78);
    control.input->setProperty("componentName", name);
    control.input->setAccessibleName(name);
    control.input->setToolTip("Используйте стрелки для шага или кнопку «Ввести…»");
    control.exactInput = new QPushButton("Ввести…");
    control.exactInput->setAccessibleName("Ввести: " + name);
    control.exactInput->setToolTip("Открыть точный ввод с подтверждением");
    header->addWidget(label);
    header->addStretch();
    header->addWidget(control.input);
    header->addWidget(control.exactInput);

    control.slider = new JumpSlider(Qt::Horizontal);
    control.slider->setAccessibleName(name);
    control.slider->setRange(0, maximum);

    layout->addLayout(header);
    layout->addWidget(control.slider);
    return row;
}

QWidget* MainWindow::createDoubleRow(const QString& name, double maximum,
                                     const QString& suffix, DoubleControl& control)
{
    auto* row = new QWidget;
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout;
    auto* label = new QLabel(name);
    control.input = new ArrowDoubleSpinBox;
    control.input->setRange(0.0, maximum);
    control.input->setDecimals(1);
    control.input->setSingleStep(0.1);
    control.input->setSuffix(suffix);
    control.input->setMinimumWidth(112);
    control.input->setProperty("componentName", name);
    control.input->setAccessibleName(name);
    control.input->setProperty("componentSuffix", suffix);
    control.input->setToolTip("Используйте стрелки для шага 0,1 или кнопку «Ввести…»");
    control.exactInput = new QPushButton("Ввести…");
    control.exactInput->setAccessibleName("Ввести: " + name);
    control.exactInput->setToolTip("Открыть точный ввод с подтверждением");
    header->addWidget(label);
    header->addStretch();
    header->addWidget(control.input);
    header->addWidget(control.exactInput);

    control.slider = new JumpSlider(Qt::Horizontal);
    control.slider->setAccessibleName(name);
    control.slider->setRange(0, static_cast<int>(std::round(maximum * control.scale)));

    layout->addLayout(header);
    layout->addWidget(control.slider);
    return row;
}

QWidget* MainWindow::createRgbPanel()
{
    auto* box = new QGroupBox("RGB");
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(createIntRow("Красный (R)", 255, red_));
    layout->addWidget(createIntRow("Зелёный (G)", 255, green_));
    layout->addWidget(createIntRow("Синий (B)", 255, blue_));
    layout->addStretch();

    auto* palette = new QPushButton("Выбрать из палитры");
    palette->setProperty("primary", true);
    connect(palette, &QPushButton::clicked, this, [this] { chooseFromPalette(); });
    layout->addWidget(palette);
    return box;
}

QWidget* MainWindow::createCmykPanel()
{
    auto* box = new QGroupBox("CMYK");
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(createDoubleRow("Голубой (C)", 100.0, "%", cyan_));
    layout->addWidget(createDoubleRow("Пурпурный (M)", 100.0, "%", magenta_));
    layout->addWidget(createDoubleRow("Жёлтый (Y)", 100.0, "%", yellow_));
    layout->addWidget(createDoubleRow("Чёрный (K)", 100.0, "%", key_));
    layout->addStretch();

    auto* palette = new QPushButton("Выбрать из палитры");
    palette->setProperty("primary", true);
    connect(palette, &QPushButton::clicked, this, [this] { chooseFromPalette(); });
    layout->addWidget(palette);
    return box;
}

QWidget* MainWindow::createHlsPanel()
{
    auto* box = new QGroupBox("HLS");
    auto* layout = new QVBoxLayout(box);
    layout->addWidget(createDoubleRow("Тон (H)", 360.0, "°", hue_));
    layout->addWidget(createDoubleRow("Светлота (L)", 100.0, "%", lightness_));
    layout->addWidget(createDoubleRow("Насыщенность (S)", 100.0, "%", saturation_));
    layout->addStretch();

    auto* palette = new QPushButton("Выбрать из палитры");
    palette->setProperty("primary", true);
    connect(palette, &QPushButton::clicked, this, [this] { chooseFromPalette(); });
    layout->addWidget(palette);
    return box;
}

void MainWindow::connectRgbControl(IntControl& control)
{
    IntControl* controlPtr = &control;
    connect(control.slider, &QSlider::valueChanged, this,
            [this, controlPtr](int value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->input);
        controlPtr->input->setValue(value);
        controller_.setRgb({red_.input->value(), green_.input->value(), blue_.input->value()});
    });

    connect(control.input, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, controlPtr](int value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->slider);
        controlPtr->slider->setValue(value);
        controller_.setRgb({red_.input->value(), green_.input->value(), blue_.input->value()});
    });

    connect(control.exactInput, &QPushButton::clicked, this,
            [this, controlPtr] {
        const QString name = controlPtr->input->property("componentName").toString();
        const auto value = requestIntValue(name, controlPtr->input->value(),
                                           controlPtr->input->minimum(),
                                           controlPtr->input->maximum());
        if (value)
        {
            RgbColor rgb = controller_.state().rgb;
            if (controlPtr == &red_) rgb.r = *value;
            else if (controlPtr == &green_) rgb.g = *value;
            else rgb.b = *value;
            controller_.setRgb(rgb);
        }
    });
}

void MainWindow::connectCmykControl(DoubleControl& control)
{
    DoubleControl* controlPtr = &control;
    connect(control.slider, &QSlider::valueChanged, this,
            [this, controlPtr](int value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->input);
        controlPtr->input->setValue(value / static_cast<double>(controlPtr->scale));
        controller_.setCmyk({cyan_.input->value(), magenta_.input->value(),
                             yellow_.input->value(), key_.input->value()});
    });

    connect(control.input, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, controlPtr](double value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->slider);
        controlPtr->slider->setValue(static_cast<int>(std::round(value * controlPtr->scale)));
        controller_.setCmyk({cyan_.input->value(), magenta_.input->value(),
                             yellow_.input->value(), key_.input->value()});
    });

    connect(control.exactInput, &QPushButton::clicked, this,
            [this, controlPtr] {
        const QString name = controlPtr->input->property("componentName").toString();
        const QString suffix = controlPtr->input->property("componentSuffix").toString();
        const auto value = requestDoubleValue(name, controlPtr->input->value(),
                                              controlPtr->input->minimum(),
                                              controlPtr->input->maximum(), suffix);
        if (value)
        {
            CmykColor cmyk {cyan_.input->value(), magenta_.input->value(),
                            yellow_.input->value(), key_.input->value()};
            if (controlPtr == &cyan_) cmyk.c = *value;
            else if (controlPtr == &magenta_) cmyk.m = *value;
            else if (controlPtr == &yellow_) cmyk.y = *value;
            else cmyk.k = *value;

            // Контроллер должен получить исходное число: иначе предупреждение
            // потеряется, в том числе при округлении до уже показанного значения.
            controller_.setCmyk(cmyk);
        }
    });
}

void MainWindow::connectHlsControl(DoubleControl& control)
{
    DoubleControl* controlPtr = &control;
    connect(control.slider, &QSlider::valueChanged, this,
            [this, controlPtr](int value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->input);
        controlPtr->input->setValue(value / static_cast<double>(controlPtr->scale));
        controller_.setHls({hue_.input->value(), lightness_.input->value(),
                            saturation_.input->value()});
    });

    connect(control.input, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, controlPtr](double value) {
        if (updating_)
            return;
        const QSignalBlocker blocker(controlPtr->slider);
        controlPtr->slider->setValue(static_cast<int>(std::round(value * controlPtr->scale)));
        controller_.setHls({hue_.input->value(), lightness_.input->value(),
                            saturation_.input->value()});
    });

    connect(control.exactInput, &QPushButton::clicked, this,
            [this, controlPtr] {
        const QString name = controlPtr->input->property("componentName").toString();
        const QString suffix = controlPtr->input->property("componentSuffix").toString();
        const auto value = requestDoubleValue(name, controlPtr->input->value(),
                                              controlPtr->input->minimum(),
                                              controlPtr->input->maximum(), suffix);
        if (value)
        {
            HlsColor hls {hue_.input->value(), lightness_.input->value(),
                          saturation_.input->value()};
            if (controlPtr == &hue_) hls.h = *value;
            else if (controlPtr == &lightness_) hls.l = *value;
            else hls.s = *value;
            controller_.setHls(hls);
        }
    });
}

std::optional<int> MainWindow::requestIntValue(const QString& component, int current,
                                               int minimum, int maximum)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Точный ввод - " + component);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        QString("Введите целое значение от %1 до %2:").arg(minimum).arg(maximum),
        &dialog));

    auto* input = new QLineEdit(QString::number(current), &dialog);
    input->setObjectName("exactValue");
    input->selectAll();
    layout->addWidget(input);

    auto* error = new QLabel(&dialog);
    error->setObjectName("inputError");
    error->setWordWrap(true);
    layout->addWidget(error);
    connect(input, &QLineEdit::textChanged, error, &QLabel::clear);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    std::optional<int> result;
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [&dialog, input, error, component, minimum, maximum, &result] {
        bool ok = false;
        const int value = input->text().trimmed().toInt(&ok);

        if (!ok)
        {
            error->setText("Введите целое число без посторонних символов.");
            input->selectAll();
            input->setFocus();
            return;
        }

        if (value < minimum || value > maximum)
        {
            error->setText(
                QString("Компонента «%1» должна находиться в диапазоне от %2 до %3.\n"
                        "Введите значение повторно.")
                    .arg(component).arg(minimum).arg(maximum));
            input->selectAll();
            input->setFocus();
            return;
        }

        result = value;
        dialog.accept();
    });

    input->setFocus();
    dialog.exec();
    return result;
}

std::optional<double> MainWindow::requestDoubleValue(const QString& component,
                                                     double current,
                                                     double minimum,
                                                     double maximum,
                                                     const QString& suffix)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Точный ввод - " + component);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        QString("Введите значение от %1 до %2 %3:")
            .arg(minimum).arg(maximum).arg(suffix),
        &dialog));

    auto* input = new QLineEdit(QString::number(current, 'f', 1), &dialog);
    input->setObjectName("exactValue");
    input->selectAll();
    layout->addWidget(input);

    auto* error = new QLabel(&dialog);
    error->setObjectName("inputError");
    error->setWordWrap(true);
    layout->addWidget(error);
    connect(input, &QLineEdit::textChanged, error, &QLabel::clear);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    std::optional<double> result;
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            [&dialog, input, error, component, minimum, maximum, &result] {
        bool ok = false;
        const double value = parseNumber(input->text(), ok);

        if (!ok || !std::isfinite(value))
        {
            error->setText(
                "Введите число. Для дробной части можно использовать точку или запятую.");
            input->selectAll();
            input->setFocus();
            return;
        }

        if (value < minimum || value > maximum)
        {
            error->setText(
                QString("Компонента «%1» должна находиться в диапазоне от %2 до %3.\n"
                        "Введите значение повторно.")
                    .arg(component).arg(minimum).arg(maximum));
            input->selectAll();
            input->setFocus();
            return;
        }

        result = value;
        dialog.accept();
    });

    input->setFocus();
    dialog.exec();
    return result;
}

void MainWindow::chooseFromPalette()
{
    const RgbColor current = controller_.state().rgb;
    const QColor initial(current.r, current.g, current.b);
    const QColor selected = QColorDialog::getColor(initial, this, "Выберите цвет");

    if (selected.isValid())
        controller_.setRgb({selected.red(), selected.green(), selected.blue()});
}

void MainWindow::showUpdate(const ColorUpdate& update)
{
    if (!red_.input)
        return;

    updating_ = true;

    auto setInt = [](IntControl& control, int value) {
        control.input->setValue(value);
        control.slider->setValue(value);
    };
    auto setDouble = [](DoubleControl& control, double value) {
        control.input->setValue(value);
        control.slider->setValue(static_cast<int>(std::round(value * control.scale)));
    };

    setInt(red_, update.state.rgb.r);
    setInt(green_, update.state.rgb.g);
    setInt(blue_, update.state.rgb.b);

    setDouble(cyan_, update.state.cmyk.c);
    setDouble(magenta_, update.state.cmyk.m);
    setDouble(yellow_, update.state.cmyk.y);
    setDouble(key_, update.state.cmyk.k);

    setDouble(hue_, update.state.hls.h);
    setDouble(lightness_, update.state.hls.l);
    setDouble(saturation_, update.state.hls.s);

    updating_ = false;
    updatePreview(update.state.rgb);
    notice_->setText(QString::fromStdString(update.message));
}

void MainWindow::updatePreview(const RgbColor& rgb)
{
    preview_->setStyleSheet(QString(
        "QLabel { background-color: rgb(%1, %2, %3); border: 1px solid #777; "
        "border-radius: 8px; }")
        .arg(rgb.r).arg(rgb.g).arg(rgb.b));
}
