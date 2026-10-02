#include "ImageTableModel.h"

#include <QBrush>
#include <QColor>

#include <utility>

ImageTableModel::ImageTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ImageTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

int ImageTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 8;
}

QVariant ImageTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        return {};

    const ImageMetadata& metadata = rows_[index.row()];

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case 0: return metadata.fileName;
        case 1: return metadata.format;
        case 2:
            return metadata.width > 0 && metadata.height > 0
                ? QString("%1 x %2").arg(metadata.width).arg(metadata.height)
                : QString("-");
        case 3: return resolutionText(metadata);
        case 4:
            return metadata.colorDepthText.isEmpty()
                ? (metadata.colorDepth > 0 ? QString("%1 бит").arg(metadata.colorDepth) : QString("-"))
                : metadata.colorDepthText;
        case 5: return metadata.compression.isEmpty() ? QString("-") : metadata.compression;
        case 6: return metadata.statusText;
        case 7: return metadata.details;
        default: return {};
        }
    }

    if (role == Qt::ToolTipRole)
        return metadata.filePath + (metadata.details.isEmpty() ? QString() : "\n" + metadata.details);

    if (role == Qt::BackgroundRole)
    {
        if (metadata.state == MetadataState::Corrupted)
            return QBrush(QColor(255, 238, 238));
        if (metadata.state == MetadataState::Warning)
            return QBrush(QColor(255, 248, 225));
    }

    return {};
}

QVariant ImageTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    static const QString headers[] = {
        "Имя файла",
        "Формат",
        "Размер, px",
        "Разрешение, dpi",
        "Глубина цвета",
        "Сжатие",
        "Статус",
        "Дополнительные данные"
    };

    if (section >= 0 && section < 8)
        return headers[section];
    return {};
}

void ImageTableModel::clear()
{
    beginResetModel();
    rows_.clear();
    corruptedCount_ = 0;
    warningCount_ = 0;
    endResetModel();
}

void ImageTableModel::appendBatch(QVector<ImageMetadata> batch)
{
    if (batch.isEmpty())
        return;

    const int first = rows_.size();
    const int last = first + batch.size() - 1;
    beginInsertRows(QModelIndex(), first, last);

    rows_.reserve(rows_.size() + batch.size());
    for (ImageMetadata& metadata : batch)
    {
        if (metadata.state == MetadataState::Corrupted)
            ++corruptedCount_;
        else if (metadata.state == MetadataState::Warning)
            ++warningCount_;
        rows_.push_back(std::move(metadata));
    }

    endInsertRows();
}

const ImageMetadata* ImageTableModel::metadataAt(int row) const
{
    if (row < 0 || row >= rows_.size())
        return nullptr;
    return &rows_[row];
}

int ImageTableModel::corruptedCount() const
{
    return corruptedCount_;
}

int ImageTableModel::warningCount() const
{
    return warningCount_;
}

QString ImageTableModel::resolutionText(const ImageMetadata& metadata) const
{
    if (metadata.dpiX <= 0.0 || metadata.dpiY <= 0.0)
        return "-";

    return QString("%1 x %2")
        .arg(metadata.dpiX, 0, 'f', 2)
        .arg(metadata.dpiY, 0, 'f', 2);
}
