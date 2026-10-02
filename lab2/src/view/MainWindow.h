#pragma once

#include "model/ImageTableModel.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QStringList>

#include <atomic>
#include <memory>

class QCheckBox;
class QModelIndex;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableView;

struct DiscoveryResult
{
    QStringList files;
    QString error;
    bool cancelled{};
};

class MainWindow : public QMainWindow
{
public:
    MainWindow();
    ~MainWindow() override;

private:
    void chooseFolder();
    void startScan();
    void cancelScan();
    void startDiscovery();
    void startParsing(const QStringList& files);
    void finishScan();
    void setBusy(bool busy);
    void showMetadata(const QModelIndex& current);
    void clearPreview();

    QLineEdit* folderEdit_{};
    QCheckBox* recursiveCheck_{};
    QPushButton* chooseButton_{};
    QPushButton* scanButton_{};
    QPushButton* cancelButton_{};
    QProgressBar* progressBar_{};
    QLabel* statusLabel_{};
    QTableView* tableView_{};
    QLabel* previewLabel_{};
    QLabel* selectedFileLabel_{};
    QLabel* selectedInfoLabel_{};

    ImageTableModel model_;
    QFutureWatcher<DiscoveryResult> discoveryWatcher_;
    QFutureWatcher<ImageMetadata> parseWatcher_;
    std::shared_ptr<std::atomic_bool> cancelToken_;
    QElapsedTimer elapsed_;
    bool busy_{};
};
