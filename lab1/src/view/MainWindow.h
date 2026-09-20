#pragma once

#include "controller/ColorController.h"

#include <QMainWindow>
#include <optional>

class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QString;
class QWidget;

class MainWindow : public QMainWindow
{
public:
    MainWindow();

private:
    struct IntControl
    {
        QSlider* slider{};
        QSpinBox* input{};
        QPushButton* exactInput{};
    };

    struct DoubleControl
    {
        QSlider* slider{};
        QDoubleSpinBox* input{};
        QPushButton* exactInput{};
        int scale{10};
    };

    QWidget* createRgbPanel();
    QWidget* createCmykPanel();
    QWidget* createHlsPanel();
    QWidget* createIntRow(const QString& name, int maximum, IntControl& control);
    QWidget* createDoubleRow(const QString& name, double maximum,
                             const QString& suffix, DoubleControl& control);

    void connectRgbControl(IntControl& control);
    void connectCmykControl(DoubleControl& control);
    void connectHlsControl(DoubleControl& control);
    std::optional<int> requestIntValue(const QString& component, int current,
                                       int minimum, int maximum);
    std::optional<double> requestDoubleValue(const QString& component, double current,
                                             double minimum, double maximum,
                                             const QString& suffix);
    void chooseFromPalette();
    void showUpdate(const ColorUpdate& update);
    void updatePreview(const RgbColor& rgb);

    ColorController controller_;
    bool updating_{false};

    IntControl red_;
    IntControl green_;
    IntControl blue_;

    DoubleControl cyan_;
    DoubleControl magenta_;
    DoubleControl yellow_;
    DoubleControl key_;

    DoubleControl hue_;
    DoubleControl lightness_;
    DoubleControl saturation_;

    QLabel* preview_{};
    QLabel* notice_{};
};
