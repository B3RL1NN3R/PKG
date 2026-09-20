#include "core/ColorConverter.h"
#include "controller/ColorController.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include "view/MainWindow.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTest>
#include <QTimer>

namespace
{
int failures = 0;
int checks = 0;

void require(bool condition, const std::string& name)
{
    ++checks;
    if (!condition)
    {
        std::cerr << "FAILED: " << name << '\n';
        ++failures;
    }
}

void requireNear(double actual, double expected, double epsilon,
                 const std::string& name)
{
    require(std::abs(actual - expected) <= epsilon, name);
}

void requireRgb(const RgbColor& actual, const RgbColor& expected,
                const std::string& name)
{
    require(actual.r == expected.r && actual.g == expected.g && actual.b == expected.b,
            name);
}
}

static int runCoreChecks()
{
    failures = 0;
    checks = 0;
    constexpr double eps = 1e-6;

    const CmykColor redCmyk = ColorConverter::rgbToCmyk({255, 0, 0});
    requireNear(redCmyk.c, 0.0, eps, "red RGB -> CMYK: C");
    requireNear(redCmyk.m, 100.0, eps, "red RGB -> CMYK: M");
    requireNear(redCmyk.y, 100.0, eps, "red RGB -> CMYK: Y");
    requireNear(redCmyk.k, 0.0, eps, "red RGB -> CMYK: K");

    const HlsColor redHls = ColorConverter::rgbToHls({255, 0, 0});
    requireNear(redHls.h, 0.0, eps, "red RGB -> HLS: H");
    requireNear(redHls.l, 50.0, eps, "red RGB -> HLS: L");
    requireNear(redHls.s, 100.0, eps, "red RGB -> HLS: S");

    requireRgb(ColorConverter::cmykToRgb({0.0, 100.0, 100.0, 0.0}).color,
               {255, 0, 0}, "red CMYK -> RGB");
    requireRgb(ColorConverter::hlsToRgb({0.0, 50.0, 100.0}).color,
               {255, 0, 0}, "red HLS -> RGB");

    const CmykColor blackCmyk = ColorConverter::rgbToCmyk({0, 0, 0});
    requireNear(blackCmyk.c, 0.0, eps, "black RGB -> CMYK: C");
    requireNear(blackCmyk.m, 0.0, eps, "black RGB -> CMYK: M");
    requireNear(blackCmyk.y, 0.0, eps, "black RGB -> CMYK: Y");
    requireNear(blackCmyk.k, 100.0, eps, "black RGB -> CMYK: K");

    const HlsColor whiteHls = ColorConverter::rgbToHls({255, 255, 255});
    requireNear(whiteHls.h, 0.0, eps, "white RGB -> HLS: H");
    requireNear(whiteHls.l, 100.0, eps, "white RGB -> HLS: L");
    requireNear(whiteHls.s, 0.0, eps, "white RGB -> HLS: S");

    const RgbColor samples[] = {
        {0, 0, 0}, {255, 255, 255}, {255, 0, 0},
        {0, 255, 0}, {0, 0, 255}, {128, 128, 128},
        {12, 123, 231}, {73, 19, 201}, {240, 200, 17}
    };

    for (const RgbColor& sample : samples)
    {
        requireRgb(ColorConverter::cmykToRgb(ColorConverter::rgbToCmyk(sample)).color,
                   sample, "RGB -> CMYK -> RGB round trip");
        requireRgb(ColorConverter::hlsToRgb(ColorConverter::rgbToHls(sample)).color,
                   sample, "RGB -> HLS -> RGB round trip");
    }

    const auto clamped = ColorConverter::cmykToRgb({-10.0, 120.0, 50.0, 0.0});
    require(clamped.clamped, "CMYK invalid input is reported as clamped");
    requireRgb(clamped.color, {255, 0, 128}, "CMYK invalid input is safely clamped");

    int updateCount = 0;
    ColorUpdate lastUpdate{};
    ColorController controller([&](const ColorUpdate& update) {
        ++updateCount;
        lastUpdate = update;
    });
    controller.setHls({240.0, 50.0, 100.0});
    require(updateCount == 1, "controller publishes the changed state");
    requireRgb(lastUpdate.state.rgb, {0, 0, 255}, "controller recalculates HLS -> RGB");
    requireNear(lastUpdate.state.cmyk.c, 100.0, eps, "controller recalculates RGB -> CMYK: C");
    requireNear(lastUpdate.state.cmyk.m, 100.0, eps, "controller recalculates RGB -> CMYK: M");
    requireNear(lastUpdate.state.cmyk.y, 0.0, eps, "controller recalculates RGB -> CMYK: Y");
    requireNear(lastUpdate.state.cmyk.k, 0.0, eps, "controller recalculates RGB -> CMYK: K");

    controller.setCmyk({12.35, 23.44, 34.56, 4.321});
    requireNear(lastUpdate.state.cmyk.c, 12.4, eps,
                "controller rounds CMYK half up: C");
    requireNear(lastUpdate.state.cmyk.m, 23.4, eps,
                "controller rounds CMYK down: M");
    requireNear(lastUpdate.state.cmyk.y, 34.6, eps,
                "controller rounds CMYK up: Y");
    requireNear(lastUpdate.state.cmyk.k, 4.3, eps,
                "controller rounds CMYK to one decimal: K");

    controller.setHls({217.16, 43.24, 71.65});
    requireNear(lastUpdate.state.hls.h, 217.2, eps,
                "controller rounds HLS up: H");
    requireNear(lastUpdate.state.hls.l, 43.2, eps,
                "controller rounds HLS down: L");
    requireNear(lastUpdate.state.hls.s, 71.7, eps,
                "controller rounds HLS half up: S");

    controller.setHls({500.0, -10.0, 150.0});
    requireNear(lastUpdate.state.hls.h, 360.0, eps,
                "controller clamps HLS: H");
    requireNear(lastUpdate.state.hls.l, 0.0, eps,
                "controller clamps HLS: L");
    requireNear(lastUpdate.state.hls.s, 100.0, eps,
                "controller clamps HLS: S");
    require(!lastUpdate.message.empty(), "controller reports HLS clamping");

    // Precision loss must be reported independently of clamping and RGB rounding.
    controller.setCmyk({12.35, 0.0, 0.0, 0.0});
    require(lastUpdate.message.find("CMYK округлено") != std::string::npos,
            "controller reports original CMYK input rounding");
    require(lastUpdate.message.find("RGB округлён") != std::string::npos,
            "controller also reports RGB channel rounding");
    const int beforeRepeatedInput = updateCount;
    controller.setCmyk({12.36, 0.0, 0.0, 0.0});
    require(updateCount == beforeRepeatedInput + 1,
            "same rounded CMYK value still publishes a notification");
    require(lastUpdate.message.find("CMYK округлено") != std::string::npos,
            "same rounded CMYK value still reports precision loss");
    requireNear(lastUpdate.state.cmyk.c, 12.4, eps,
                "CMYK input survives RGB quantization");

    controller.setHls({217.16, 50.0, 100.0});
    require(lastUpdate.message.find("HLS округлено") != std::string::npos,
            "controller reports original HLS input rounding");
    controller.setHls({217.19, 50.0, 100.0});
    requireNear(lastUpdate.state.hls.h, 217.2, eps,
                "same rounded HLS value is retained");
    require(lastUpdate.message.find("HLS округлено") != std::string::npos,
            "same rounded HLS value still reports precision loss");

    controller.setCmyk({-10.0, 23.44, 34.56, 4.321});
    require(lastUpdate.message.find("ограничены") != std::string::npos &&
            lastUpdate.message.find("CMYK округлено") != std::string::npos,
            "clamping does not hide input rounding");
    controller.setRgb({12, 123, 231});
    require(lastUpdate.message.find("показаны с округлением") != std::string::npos,
            "rounding of calculated display values is reported");
    controller.setRgb({255, 0, 0});
    require(lastUpdate.message.empty(), "exact conversion clears a stale warning");
    controller.setRgb({300, -20, 0});
    requireRgb(lastUpdate.state.rgb, {255, 0, 0}, "RGB input is clamped");
    require(lastUpdate.message.find("ограничены") != std::string::npos,
            "RGB clamping is reported");

    const auto fullTurn = ColorConverter::hlsToRgb({360.0, 50.0, 100.0});
    requireRgb(fullTurn.color, {255, 0, 0}, "hue 360 equals hue 0");
    require(!fullTurn.clamped && !fullTurn.rounded,
            "valid hue endpoint is not reported as corrected");
    const auto gray = ColorConverter::hlsToRgb({217.0, 50.0, 0.0});
    requireRgb(gray.color, {128, 128, 128}, "zero saturation produces gray");
    require(gray.rounded, "fractional gray channels report rounding");
    requireRgb(ColorConverter::cmykToRgb({30.0, 70.0, 90.0, 100.0}).color,
               {0, 0, 0}, "full key produces black for every CMY combination");

    const double nan = std::numeric_limits<double>::quiet_NaN();
    controller.setCmyk({nan, 0.0, 0.0, 0.0});
    requireRgb(lastUpdate.state.rgb, {255, 0, 0}, "NaN input preserves the previous color");
    require(lastUpdate.message.find("конечными") != std::string::npos,
            "NaN input is rejected with an explanation");
    controller.setHls({0.0, std::numeric_limits<double>::infinity(), 0.0});
    requireRgb(lastUpdate.state.rgb, {255, 0, 0}, "infinite input preserves the previous color");
    require(ColorConverter::cmykToRgb({nan, 0.0, 0.0, 0.0}).clamped,
            "core reports invalid non-finite CMYK input");
    require(ColorConverter::hlsToRgb({nan, 50.0, 100.0}).clamped,
            "core reports invalid non-finite HLS input");

    if (failures == 0)
    {
        std::cout << "All " << checks << " ColorConverter checks passed.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}

namespace
{
template<class Widget>
Widget* componentWidget(MainWindow& window, const QString& name)
{
    for (auto* widget : window.findChildren<Widget*>())
        if (widget->accessibleName() == name)
            return widget;
    return nullptr;
}

bool enterValue(MainWindow& window, const QString& component, const QString& text)
{
    auto* button = componentWidget<QPushButton>(window, "Ввести: " + component);
    if (!button)
        return false;

    bool handled = false;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [] {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
            dialog->reject();
    });
    watchdog.start(3000);
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog)
            return;
        auto* input = dialog->findChild<QLineEdit*>("exactValue");
        auto* buttons = dialog->findChild<QDialogButtonBox*>();
        if (!input || !buttons)
            return;
        input->setText(text);
        buttons->button(QDialogButtonBox::Ok)->click();
        handled = dialog->result() == QDialog::Accepted;
    });
    button->click();
    return handled;
}
}

class ColorConverterTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void conversionsAndController()
    {
        QCOMPARE(runCoreChecks(), EXIT_SUCCESS);
    }

    void exactInputRounding_data()
    {
        QTest::addColumn<QString>("component");
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("expected");
        QTest::addColumn<QString>("warning");
        QTest::newRow("CMYK-comma") << QString("Голубой (C)") << QString("12,35")
            << 12.4 << QString("CMYK округлено");
        QTest::newRow("HLS-dot") << QString("Тон (H)") << QString("217.16")
            << 217.2 << QString("HLS округлено");
        QTest::newRow("same-CMYK") << QString("Голубой (C)") << QString("0.01")
            << 0.0 << QString("CMYK округлено");
        QTest::newRow("same-HLS") << QString("Тон (H)") << QString("0.01")
            << 0.0 << QString("HLS округлено");
    }

    void exactInputRounding()
    {
        QFETCH(QString, component);
        QFETCH(QString, text);
        QFETCH(double, expected);
        QFETCH(QString, warning);
        MainWindow window;
        window.show();
        QVERIFY(enterValue(window, component, text));
        auto* input = componentWidget<QDoubleSpinBox>(window, component);
        auto* slider = componentWidget<QSlider>(window, component);
        auto* notice = window.findChild<QLabel*>("conversionNotice");
        QVERIFY(input && slider && notice);
        QCOMPARE(input->value(), expected);
        QCOMPARE(slider->value(), qRound(expected * 10));
        QVERIFY(notice->isVisible());
        QVERIFY(notice->text().contains(warning));
    }

    void rgbInputClearsWarning()
    {
        MainWindow window;
        window.show();
        QVERIFY(enterValue(window, "Голубой (C)", "0.01"));
        QVERIFY(!window.findChild<QLabel*>("conversionNotice")->text().isEmpty());
        // Re-entering the same RGB channel must still clear the obsolete message.
        QVERIFY(enterValue(window, "Красный (R)", "255"));
        QVERIFY(window.findChild<QLabel*>("conversionNotice")->text().isEmpty());
    }

    void slidersSynchronize_data()
    {
        QTest::addColumn<QString>("component");
        QTest::addColumn<int>("value");
        QTest::addColumn<int>("red");
        QTest::addColumn<int>("green");
        QTest::addColumn<int>("blue");
        QTest::newRow("RGB") << QString("Зелёный (G)") << 255 << 255 << 255 << 0;
        QTest::newRow("CMYK") << QString("Голубой (C)") << 1000 << 0 << 0 << 0;
        QTest::newRow("HLS") << QString("Тон (H)") << 2400 << 0 << 0 << 255;
    }

    void slidersSynchronize()
    {
        QFETCH(QString, component);
        QFETCH(int, value);
        QFETCH(int, red);
        QFETCH(int, green);
        QFETCH(int, blue);
        MainWindow window;
        auto* slider = componentWidget<QSlider>(window, component);
        QVERIFY(slider);
        slider->setValue(value);
        QCOMPARE(componentWidget<QSpinBox>(window, "Красный (R)")->value(), red);
        QCOMPARE(componentWidget<QSpinBox>(window, "Зелёный (G)")->value(), green);
        QCOMPARE(componentWidget<QSpinBox>(window, "Синий (B)")->value(), blue);
        auto* hue = componentWidget<QDoubleSpinBox>(window, "Тон (H)");
        auto* lightness = componentWidget<QDoubleSpinBox>(window, "Светлота (L)");
        if (red == 255 && green == 255)
        {
            QCOMPARE(hue->value(), 60.0);
            QCOMPARE(componentWidget<QDoubleSpinBox>(window, "Жёлтый (Y)")->value(), 100.0);
        }
        else if (blue == 255)
        {
            QCOMPARE(hue->value(), 240.0);
            QCOMPARE(componentWidget<QDoubleSpinBox>(window, "Голубой (C)")->value(), 100.0);
        }
        else
            QCOMPARE(lightness->value(), 0.0);
    }

    void invalidInput_data()
    {
        QTest::addColumn<QString>("component");
        QTest::addColumn<QString>("text");
        QTest::newRow("RGB-range") << QString("Красный (R)") << QString("256");
        QTest::newRow("RGB-fraction") << QString("Красный (R)") << QString("12.5");
        QTest::newRow("CMYK-range") << QString("Голубой (C)") << QString("-0.1");
        QTest::newRow("HLS-range") << QString("Тон (H)") << QString("361");
        QTest::newRow("not-number") << QString("Тон (H)") << QString("abc");
        QTest::newRow("NaN") << QString("Тон (H)") << QString("nan");
    }

    void invalidInput()
    {
        QFETCH(QString, component);
        QFETCH(QString, text);
        MainWindow window;
        window.show();
        bool rejected = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                qFatal("Exact input dialog did not open");
            auto* input = dialog->findChild<QLineEdit*>("exactValue");
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            input->setText(text);
            buttons->button(QDialogButtonBox::Ok)->click();
            rejected = dialog->isVisible() &&
                       !dialog->findChild<QLabel*>("inputError")->text().isEmpty();
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
        auto* button = componentWidget<QPushButton>(window, "Ввести: " + component);
        QVERIFY(button);
        button->click();
        QVERIFY(rejected);
        QCOMPARE(componentWidget<QSpinBox>(window, "Красный (R)")->value(), 255);
        QCOMPARE(componentWidget<QDoubleSpinBox>(window, "Тон (H)")->value(), 0.0);
    }

    void paletteSynchronizesAllPanels()
    {
        MainWindow window;
        window.show();
        const QColor colors[] = {QColor(0, 0, 255), QColor(0, 255, 0), QColor(255, 255, 0)};
        const double hues[] = {240.0, 120.0, 60.0};
        const double cyan[] = {100.0, 100.0, 0.0};
        int paletteButtons = 0;
        for (auto* button : window.findChildren<QPushButton*>())
        {
            if (button->text() != "Выбрать из палитры")
                continue;
            QVERIFY(paletteButtons < 3);
            const QColor selected = colors[paletteButtons];
            QTimer::singleShot(0, &window, [&] {
                auto* palette = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
                if (!palette)
                    qFatal("Color dialog did not open");
                palette->setCurrentColor(selected);
                palette->accept();
            });
            button->click();
            QCOMPARE(componentWidget<QSpinBox>(window, "Красный (R)")->value(), selected.red());
            QCOMPARE(componentWidget<QSpinBox>(window, "Зелёный (G)")->value(), selected.green());
            QCOMPARE(componentWidget<QSpinBox>(window, "Синий (B)")->value(), selected.blue());
            QCOMPARE(componentWidget<QDoubleSpinBox>(window, "Тон (H)")->value(), hues[paletteButtons]);
            QCOMPARE(componentWidget<QDoubleSpinBox>(window, "Голубой (C)")->value(), cyan[paletteButtons]);
            ++paletteButtons;
        }
        QCOMPARE(paletteButtons, 3);
    }
};

QTEST_MAIN(ColorConverterTests)
#include "ColorConverterTests.moc"
