#pragma once

#include "model/ImageMetadata.h"

#include <QString>

class ImageParser
{
public:
    static ImageMetadata parseFile(const QString& filePath);
};
