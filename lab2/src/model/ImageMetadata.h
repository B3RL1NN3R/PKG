#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

enum class MetadataState
{
    Ok,
    Warning,
    Corrupted
};

struct ImageMetadata
{
    QString fileName;
    QString filePath;
    QString format;
    int width{};
    int height{};
    double dpiX{};
    double dpiY{};
    bool windowsDefaultDpi{};
    int colorDepth{};
    QString colorDepthText;
    QString compression;
    QString statusText{"OK"};
    QString details;
    MetadataState state{MetadataState::Ok};
};

Q_DECLARE_METATYPE(ImageMetadata)
Q_DECLARE_METATYPE(QVector<ImageMetadata>)
