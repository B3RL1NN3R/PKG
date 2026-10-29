#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QPixmap>
#include <memory>
#include "model/ImageBuffer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onLoadImage();
    void onApply();
    void onSave();
    void onEdgeModeChanged(int index);
    void onRadiusChanged(int value);
    void onMethodChanged(int index);
    void onProcessingFinished();

private:
    void runProcessing();
    void rescaleImages();
    bool validateParameters();
    void showWarning(const QString& message);
    int getOptimalRadius(int method) const;
    int getMaxRecommendedRadius(int method) const;

    QLabel* m_lblOriginal;
    QLabel* m_lblResult;
    QProgressBar* m_progressBar;
    QLabel* m_lblStatus;

    QComboBox* m_cbMethod;
    QComboBox* m_cbEdge;
    QLabel* m_lblEdgeDesc;
    QSlider* m_sliderRadius;
    QLabel* m_lblRadiusValue;

    QPushButton* m_btnLoad;
    QPushButton* m_btnApply;
    QPushButton* m_btnSave;

    std::unique_ptr<ImageBuffer> m_source;
    QPixmap m_originalPixmap;
    QPixmap m_resultPixmap;
    QFutureWatcher<ImageBuffer> m_watcher;
};