#pragma once

#include "ImageMetadata.h"

#include <QAbstractTableModel>
#include <QVector>

class ImageTableModel : public QAbstractTableModel
{
public:
    explicit ImageTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void clear();
    void appendBatch(QVector<ImageMetadata> batch);
    const ImageMetadata* metadataAt(int row) const;
    int corruptedCount() const;
    int warningCount() const;

private:
    QString resolutionText(const ImageMetadata& metadata) const;

    QVector<ImageMetadata> rows_;
    int corruptedCount_{};
    int warningCount_{};
};
