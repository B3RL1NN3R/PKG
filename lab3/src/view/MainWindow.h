#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QFutureWatcher>
#include <QPixmap>
#include <QCloseEvent>
#include <QKeyEvent>
#include <atomic>
#include <memory>
#include "model/ImageBuffer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

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
    void setProcessingUiLocked(bool locked);
    void rescaleImages();
    bool validateParameters();
    void showWarning(const QString& message);
    int getOptimalRadius(int method) const;
    int getMaxRecommendedRadius(int method) const;

    QWidget* m_controlPanel = nullptr;
    QLabel* m_lblOriginal;
    QLabel* m_lblResult;
    QProgressBar* m_progressBar;
    QLabel* m_lblStatus;

    QComboBox* m_cbMethod;
    QComboBox* m_cbEdge;
    QComboBox* m_cbStructuringElement;
    QLabel* m_lblEdgeDesc;
    QSlider* m_sliderRadius;
    QLabel* m_lblRadiusValue;

    QPushButton* m_btnLoad;
    QPushButton* m_btnApply;
    QPushButton* m_btnSave;

    std::unique_ptr<ImageBuffer> m_source;
    ImageBuffer m_resultBuffer; // Raw processing output; preview enhancement never changes saved data.
    QPixmap m_originalPixmap;
    QPixmap m_resultPixmap;
    QFutureWatcher<ImageBuffer> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    bool m_isProcessing = false;
    bool m_isClosing = false;
};