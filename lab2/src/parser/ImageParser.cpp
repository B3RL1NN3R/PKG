#include "ImageParser.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double WindowsDefaultDpi = 96.0;

quint16 readLe16(const char* data)
{
    return static_cast<quint16>(static_cast<unsigned char>(data[0])) |
           static_cast<quint16>(static_cast<unsigned char>(data[1]) << 8);
}

quint32 readLe32(const char* data)
{
    return static_cast<quint32>(static_cast<unsigned char>(data[0])) |
           (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[3])) << 24);
}

qint32 readLeS32(const char* data)
{
    return static_cast<qint32>(readLe32(data));
}

quint16 readBe16(const char* data)
{
    return (static_cast<quint16>(static_cast<unsigned char>(data[0])) << 8) |
           static_cast<quint16>(static_cast<unsigned char>(data[1]));
}

quint32 readBe32(const char* data)
{
    return (static_cast<quint32>(static_cast<unsigned char>(data[0])) << 24) |
           (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 8) |
           static_cast<quint32>(static_cast<unsigned char>(data[3]));
}

void appendDetail(ImageMetadata& metadata, const QString& text)
{
    if (text.isEmpty())
        return;
    if (!metadata.details.isEmpty())
        metadata.details += "; ";
    metadata.details += text;
}

void setWarning(ImageMetadata& metadata, const QString& text)
{
    if (metadata.state != MetadataState::Corrupted)
    {
        metadata.state = MetadataState::Warning;
        metadata.statusText = "Предупреждение";
    }
    appendDetail(metadata, text);
}

void setCorrupted(ImageMetadata& metadata, const QString& text)
{
    metadata.state = MetadataState::Corrupted;
    metadata.statusText = "Файл поврежден";
    appendDetail(metadata, text);
}

void applyWindowsDefaults(ImageMetadata& metadata)
{
    if (metadata.state == MetadataState::Corrupted)
        return;

    if (metadata.dpiX <= 0.0 || metadata.dpiY <= 0.0)
    {
        metadata.dpiX = WindowsDefaultDpi;
        metadata.dpiY = WindowsDefaultDpi;
        metadata.windowsDefaultDpi = true;
        appendDetail(metadata, "Физическое разрешение отсутствует в файле: используется значение Windows по умолчанию 96 dpi");
    }
}

QString expectedFormatForSuffix(const QString& suffix)
{
    const QString ext = suffix.toLower();
    if (ext == "jpg" || ext == "jpeg") return "JPEG";
    if (ext == "gif") return "GIF";
    if (ext == "tif" || ext == "tiff") return "TIFF";
    if (ext == "bmp") return "BMP";
    if (ext == "png") return "PNG";
    if (ext == "pcx") return "PCX";
    return {};
}

QString detectFormat(const QByteArray& header)
{
    if (header.size() >= 8 &&
        static_cast<unsigned char>(header[0]) == 0x89 &&
        header.mid(1, 7) == QByteArray("PNG\r\n\x1a\n", 7))
        return "PNG";

    if (header.size() >= 3 &&
        static_cast<unsigned char>(header[0]) == 0xFF &&
        static_cast<unsigned char>(header[1]) == 0xD8 &&
        static_cast<unsigned char>(header[2]) == 0xFF)
        return "JPEG";

    if (header.size() >= 6 &&
        (header.left(6) == "GIF87a" || header.left(6) == "GIF89a"))
        return "GIF";

    if (header.size() >= 4 &&
        ((header[0] == 'I' && header[1] == 'I' &&
          static_cast<unsigned char>(header[2]) == 42 && static_cast<unsigned char>(header[3]) == 0) ||
         (header[0] == 'M' && header[1] == 'M' &&
          static_cast<unsigned char>(header[2]) == 0 && static_cast<unsigned char>(header[3]) == 42)))
        return "TIFF";

    if (header.size() >= 2 && header[0] == 'B' && header[1] == 'M')
        return "BMP";

    if (header.size() >= 4 &&
        static_cast<unsigned char>(header[0]) == 0x0A &&
        static_cast<unsigned char>(header[1]) <= 5 &&
        static_cast<unsigned char>(header[2]) <= 1 &&
        (static_cast<unsigned char>(header[3]) == 1 ||
         static_cast<unsigned char>(header[3]) == 2 ||
         static_cast<unsigned char>(header[3]) == 4 ||
         static_cast<unsigned char>(header[3]) == 8))
        return "PCX";

    return {};
}

QString jpegCompressionName(unsigned char marker)
{
    switch (marker)
    {
    case 0xC0: return "JPEG Baseline DCT";
    case 0xC1: return "JPEG Extended Sequential DCT";
    case 0xC2: return "JPEG Progressive DCT";
    case 0xC3: return "JPEG Lossless Sequential";
    case 0xC5: return "JPEG Differential Sequential DCT";
    case 0xC6: return "JPEG Differential Progressive DCT";
    case 0xC7: return "JPEG Differential Lossless";
    case 0xC9: return "JPEG Extended Sequential Arithmetic";
    case 0xCA: return "JPEG Progressive Arithmetic";
    case 0xCB: return "JPEG Lossless Arithmetic";
    case 0xCD: return "JPEG Differential Sequential Arithmetic";
    case 0xCE: return "JPEG Differential Progressive Arithmetic";
    case 0xCF: return "JPEG Differential Lossless Arithmetic";
    default: return "JPEG";
    }
}

bool isSofMarker(unsigned char marker)
{
    switch (marker)
    {
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    case 0xC5:
    case 0xC6:
    case 0xC7:
    case 0xC9:
    case 0xCA:
    case 0xCB:
    case 0xCD:
    case 0xCE:
    case 0xCF:
        return true;
    default:
        return false;
    }
}

bool parseExifResolution(const QByteArray& app1, double& dpiX, double& dpiY)
{
    if (app1.size() < 14 || app1.left(6) != QByteArray("Exif\0\0", 6))
        return false;

    const QByteArray data = app1.mid(6);
    if (data.size() < 8)
        return false;

    const bool little = data[0] == 'I' && data[1] == 'I';
    const bool big = data[0] == 'M' && data[1] == 'M';
    if (!little && !big)
        return false;

    auto u16 = [&](int offset, quint16& value) -> bool {
        if (offset < 0 || offset + 2 > data.size())
            return false;
        value = little ? readLe16(data.constData() + offset) : readBe16(data.constData() + offset);
        return true;
    };

    auto u32 = [&](int offset, quint32& value) -> bool {
        if (offset < 0 || offset + 4 > data.size())
            return false;
        value = little ? readLe32(data.constData() + offset) : readBe32(data.constData() + offset);
        return true;
    };

    quint16 magic{};
    quint32 ifdOffset{};
    if (!u16(2, magic) || magic != 42 || !u32(4, ifdOffset))
        return false;

    if (ifdOffset > static_cast<quint32>(data.size() - 2))
        return false;

    quint16 count{};
    if (!u16(static_cast<int>(ifdOffset), count))
        return false;

    if (static_cast<quint64>(ifdOffset) + 2ULL + static_cast<quint64>(count) * 12ULL + 4ULL >
        static_cast<quint64>(data.size()))
        return false;

    double xResolution = 0.0;
    double yResolution = 0.0;
    quint16 resolutionUnit = 2;

    for (quint16 i = 0; i < count; ++i)
    {
        const int entryOffset = static_cast<int>(ifdOffset) + 2 + i * 12;
        quint16 tag{};
        quint16 type{};
        quint32 valueCount{};
        quint32 valueOrOffset{};
        if (!u16(entryOffset, tag) || !u16(entryOffset + 2, type) ||
            !u32(entryOffset + 4, valueCount) || !u32(entryOffset + 8, valueOrOffset))
            return false;

        if ((tag == 282 || tag == 283) && type == 5 && valueCount >= 1)
        {
            quint32 numerator{};
            quint32 denominator{};
            if (!u32(static_cast<int>(valueOrOffset), numerator) ||
                !u32(static_cast<int>(valueOrOffset) + 4, denominator) || denominator == 0)
                continue;
            const double value = static_cast<double>(numerator) / denominator;
            if (tag == 282) xResolution = value;
            if (tag == 283) yResolution = value;
        }
        else if (tag == 296 && type == 3 && valueCount >= 1)
        {
            quint16 value{};
            if (valueCount * 2 <= 4)
                u16(entryOffset + 8, value);
            else
                u16(static_cast<int>(valueOrOffset), value);
            resolutionUnit = value;
        }
    }

    if (xResolution <= 0.0 || yResolution <= 0.0)
        return false;

    if (resolutionUnit == 2)
    {
        dpiX = xResolution;
        dpiY = yResolution;
        return true;
    }

    if (resolutionUnit == 3)
    {
        dpiX = xResolution * 2.54;
        dpiY = yResolution * 2.54;
        return true;
    }

    return false;
}

bool jpegHasEoi(QFile& file)
{
    const qint64 size = file.size();
    const qint64 tailSize = std::min<qint64>(size, 65536);
    if (!file.seek(size - tailSize))
        return false;
    const QByteArray tail = file.read(tailSize);
    for (int i = tail.size() - 2; i >= 0; --i)
    {
        if (static_cast<unsigned char>(tail[i]) == 0xFF &&
            static_cast<unsigned char>(tail[i + 1]) == 0xD9)
            return true;
    }
    return false;
}

bool parseJpeg(QFile& file, ImageMetadata& metadata)
{
    if (file.size() < 4 || !file.seek(0) || file.read(2) != QByteArray("\xFF\xD8", 2))
    {
        setCorrupted(metadata, "Некорректный маркер SOI");
        return false;
    }

    bool dimensionsFound = false;
    double jfifDpiX = 0.0;
    double jfifDpiY = 0.0;
    double exifDpiX = 0.0;
    double exifDpiY = 0.0;
    int precision = 0;
    int components = 0;

    while (file.pos() + 1 < file.size())
    {
        char byte{};
        if (file.read(&byte, 1) != 1)
            break;

        if (static_cast<unsigned char>(byte) != 0xFF)
            continue;

        do
        {
            if (file.read(&byte, 1) != 1)
                break;
        }
        while (static_cast<unsigned char>(byte) == 0xFF);

        const unsigned char marker = static_cast<unsigned char>(byte);
        if (marker == 0x00)
            continue;
        if (marker == 0xD9)
            break;
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            continue;

        const QByteArray lengthBytes = file.read(2);
        if (lengthBytes.size() != 2)
        {
            setCorrupted(metadata, "Обрезан JPEG-сегмент");
            return false;
        }

        const quint16 segmentLength = readBe16(lengthBytes.constData());
        if (segmentLength < 2)
        {
            setCorrupted(metadata, "Некорректная длина JPEG-сегмента");
            return false;
        }

        const qint64 payloadLength = segmentLength - 2;
        if (file.pos() + payloadLength > file.size())
        {
            setCorrupted(metadata, "JPEG-сегмент выходит за границы файла");
            return false;
        }

        if (marker == 0xDA)
        {
            if (!file.seek(file.pos() + payloadLength))
            {
                setCorrupted(metadata, "Не удалось перейти к JPEG-данным");
                return false;
            }
            break;
        }

        if (marker == 0xE0 || marker == 0xE1 || isSofMarker(marker))
        {
            const QByteArray payload = file.read(payloadLength);
            if (payload.size() != payloadLength)
            {
                setCorrupted(metadata, "Обрезан JPEG-сегмент");
                return false;
            }

            if (marker == 0xE0 && payload.size() >= 12 && payload.left(5) == QByteArray("JFIF\0", 5))
            {
                const unsigned char units = static_cast<unsigned char>(payload[7]);
                const quint16 xDensity = readBe16(payload.constData() + 8);
                const quint16 yDensity = readBe16(payload.constData() + 10);
                if (xDensity > 0 && yDensity > 0)
                {
                    if (units == 1)
                    {
                        jfifDpiX = xDensity;
                        jfifDpiY = yDensity;
                    }
                    else if (units == 2)
                    {
                        jfifDpiX = xDensity * 2.54;
                        jfifDpiY = yDensity * 2.54;
                    }
                }
            }
            else if (marker == 0xE1)
            {
                parseExifResolution(payload, exifDpiX, exifDpiY);
            }
            else if (isSofMarker(marker) && payload.size() >= 6)
            {
                precision = static_cast<unsigned char>(payload[0]);
                metadata.height = readBe16(payload.constData() + 1);
                metadata.width = readBe16(payload.constData() + 3);
                components = static_cast<unsigned char>(payload[5]);
                metadata.colorDepth = precision * components;
                metadata.colorDepthText = QString("%1 бит (%2 x %3 бит)")
                    .arg(metadata.colorDepth).arg(components).arg(precision);
                metadata.compression = jpegCompressionName(marker);
                dimensionsFound = metadata.width > 0 && metadata.height > 0;
            }
        }
        else if (!file.seek(file.pos() + payloadLength))
        {
            setCorrupted(metadata, "Не удалось пропустить JPEG-сегмент");
            return false;
        }
    }

    if (!dimensionsFound)
    {
        setCorrupted(metadata, "Не найден маркер SOF с размером изображения");
        return false;
    }

    if (!jpegHasEoi(file))
    {
        setCorrupted(metadata, "Отсутствует маркер EOI FF D9");
        return false;
    }

    if (exifDpiX > 0.0 && exifDpiY > 0.0)
    {
        metadata.dpiX = exifDpiX;
        metadata.dpiY = exifDpiY;
        appendDetail(metadata, "Разрешение из EXIF IFD0");
    }
    else if (jfifDpiX > 0.0 && jfifDpiY > 0.0)
    {
        metadata.dpiX = jfifDpiX;
        metadata.dpiY = jfifDpiY;
        appendDetail(metadata, "Разрешение из JFIF");
    }

    appendDetail(metadata, QString("Компонентов: %1; точность: %2 бит/компонент")
        .arg(components).arg(precision));
    return true;
}

bool parsePng(QFile& file, ImageMetadata& metadata)
{
    static const QByteArray signature("\x89PNG\r\n\x1a\n", 8);
    if (file.size() < 20 || !file.seek(0) || file.read(8) != signature)
    {
        setCorrupted(metadata, "Некорректная PNG-сигнатура");
        return false;
    }

    bool ihdrFound = false;
    bool idatFound = false;
    bool iendFound = false;
    bool firstChunk = true;
    int paletteColors = 0;
    int pngColorType = -1;

    while (file.pos() + 12 <= file.size())
    {
        const QByteArray lengthBytes = file.read(4);
        const QByteArray type = file.read(4);
        if (lengthBytes.size() != 4 || type.size() != 4)
        {
            setCorrupted(metadata, "Обрезан заголовок PNG-чанка");
            return false;
        }

        const quint32 length = readBe32(lengthBytes.constData());
        const quint64 dataStart = static_cast<quint64>(file.pos());
        const quint64 requiredEnd = dataStart + static_cast<quint64>(length) + 4ULL;
        if (requiredEnd > static_cast<quint64>(file.size()))
        {
            setCorrupted(metadata, "PNG-чанк выходит за границы файла");
            return false;
        }

        if (firstChunk && type != "IHDR")
        {
            setCorrupted(metadata, "Первый PNG-чанк не IHDR");
            return false;
        }
        firstChunk = false;

        if (type == "IHDR")
        {
            if (ihdrFound || length != 13)
            {
                setCorrupted(metadata, "Некорректный IHDR");
                return false;
            }

            const QByteArray data = file.read(13);
            if (data.size() != 13)
            {
                setCorrupted(metadata, "Обрезан IHDR");
                return false;
            }

            metadata.width = static_cast<int>(readBe32(data.constData()));
            metadata.height = static_cast<int>(readBe32(data.constData() + 4));
            const int bitDepth = static_cast<unsigned char>(data[8]);
            const int colorType = static_cast<unsigned char>(data[9]);
            const int compressionMethod = static_cast<unsigned char>(data[10]);
            const int filterMethod = static_cast<unsigned char>(data[11]);
            const int interlaceMethod = static_cast<unsigned char>(data[12]);
            pngColorType = colorType;

            const bool bitDepthValid =
                (colorType == 0 && (bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8 || bitDepth == 16)) ||
                (colorType == 2 && (bitDepth == 8 || bitDepth == 16)) ||
                (colorType == 3 && (bitDepth == 1 || bitDepth == 2 || bitDepth == 4 || bitDepth == 8)) ||
                (colorType == 4 && (bitDepth == 8 || bitDepth == 16)) ||
                (colorType == 6 && (bitDepth == 8 || bitDepth == 16));
            if (!bitDepthValid)
            {
                setCorrupted(metadata, "Недопустимая комбинация PNG bit depth и color type");
                return false;
            }

            int channels = 0;
            QString colorTypeName;
            switch (colorType)
            {
            case 0: channels = 1; colorTypeName = "градации серого"; break;
            case 2: channels = 3; colorTypeName = "RGB"; break;
            case 3: channels = 1; colorTypeName = "индексированный"; break;
            case 4: channels = 2; colorTypeName = "серый + alpha"; break;
            case 6: channels = 4; colorTypeName = "RGBA"; break;
            default:
                setCorrupted(metadata, "Неизвестный PNG color type");
                return false;
            }

            metadata.colorDepth = bitDepth * channels;
            metadata.colorDepthText = colorType == 3
                ? QString("%1 бит, индексированный").arg(bitDepth)
                : QString("%1 бит (%2 x %3 бит)").arg(metadata.colorDepth).arg(channels).arg(bitDepth);
            metadata.compression = compressionMethod == 0
                ? "DEFLATE (PNG method 0)"
                : QString("Неизвестный метод %1").arg(compressionMethod);
            appendDetail(metadata, QString("Тип цвета: %1; фильтрация: %2; interlace: %3")
                .arg(colorTypeName).arg(filterMethod).arg(interlaceMethod));

            if (metadata.width <= 0 || metadata.height <= 0 || compressionMethod != 0 ||
                filterMethod != 0 || (interlaceMethod != 0 && interlaceMethod != 1))
            {
                setCorrupted(metadata, "Некорректные обязательные поля IHDR");
                return false;
            }

            ihdrFound = true;
        }
        else if (type == "pHYs")
        {
            if (length == 9)
            {
                const QByteArray data = file.read(9);
                if (data.size() != 9)
                {
                    setCorrupted(metadata, "Обрезан pHYs");
                    return false;
                }
                const quint32 xPpm = readBe32(data.constData());
                const quint32 yPpm = readBe32(data.constData() + 4);
                const unsigned char unit = static_cast<unsigned char>(data[8]);
                if (unit == 1 && xPpm > 0 && yPpm > 0)
                {
                    metadata.dpiX = xPpm * 0.0254;
                    metadata.dpiY = yPpm * 0.0254;
                    appendDetail(metadata, "Разрешение из pHYs");
                }
            }
            else if (!file.seek(file.pos() + length))
            {
                setCorrupted(metadata, "Не удалось пропустить pHYs");
                return false;
            }
        }
        else if (type == "PLTE")
        {
            if (length % 3 == 0)
                paletteColors = static_cast<int>(length / 3);
            if (!file.seek(file.pos() + length))
            {
                setCorrupted(metadata, "Не удалось пропустить PLTE");
                return false;
            }
        }
        else
        {
            if (type == "IDAT")
                idatFound = true;
            if (type == "IEND")
            {
                if (length != 0)
                {
                    setCorrupted(metadata, "IEND должен иметь нулевую длину");
                    return false;
                }
                iendFound = true;
            }
            if (!file.seek(file.pos() + length))
            {
                setCorrupted(metadata, "Не удалось пропустить PNG-чанк");
                return false;
            }
        }

        if (!file.seek(file.pos() + 4))
        {
            setCorrupted(metadata, "Обрезан CRC PNG-чанка");
            return false;
        }

        if (type == "IEND")
            break;
    }

    if (!ihdrFound)
    {
        setCorrupted(metadata, "Отсутствует IHDR");
        return false;
    }
    if (!idatFound)
    {
        setCorrupted(metadata, "Отсутствует IDAT");
        return false;
    }
    if (!iendFound)
    {
        setCorrupted(metadata, "Отсутствует IEND");
        return false;
    }
    if (pngColorType == 3 && paletteColors == 0)
    {
        setCorrupted(metadata, "Для индексированного PNG отсутствует PLTE");
        return false;
    }

    if (paletteColors > 0)
        appendDetail(metadata, QString("Цветов в палитре: %1").arg(paletteColors));
    return true;
}

QString bmpCompressionName(quint32 value)
{
    switch (value)
    {
    case 0: return "BI_RGB";
    case 1: return "BI_RLE8";
    case 2: return "BI_RLE4";
    case 3: return "BI_BITFIELDS";
    case 4: return "BI_JPEG";
    case 5: return "BI_PNG";
    case 6: return "BI_ALPHABITFIELDS";
    case 11: return "BI_CMYK";
    case 12: return "BI_CMYKRLE8";
    case 13: return "BI_CMYKRLE4";
    default: return QString("Неизвестно (%1)").arg(value);
    }
}

bool parseBmp(QFile& file, ImageMetadata& metadata)
{
    if (file.size() < 26 || !file.seek(0))
    {
        setCorrupted(metadata, "BMP слишком мал");
        return false;
    }

    const QByteArray fileHeader = file.read(14);
    if (fileHeader.size() != 14 || fileHeader[0] != 'B' || fileHeader[1] != 'M')
    {
        setCorrupted(metadata, "Некорректная BMP-сигнатура");
        return false;
    }

    const quint32 declaredFileSize = readLe32(fileHeader.constData() + 2);
    const quint32 pixelOffset = readLe32(fileHeader.constData() + 10);
    if (declaredFileSize > 0 && declaredFileSize > static_cast<quint64>(file.size()))
    {
        setCorrupted(metadata, "Размер файла меньше значения из BITMAPFILEHEADER");
        return false;
    }
    if (pixelOffset >= static_cast<quint64>(file.size()))
    {
        setCorrupted(metadata, "Смещение пикселей выходит за границы файла");
        return false;
    }

    const QByteArray dibSizeBytes = file.read(4);
    if (dibSizeBytes.size() != 4)
    {
        setCorrupted(metadata, "Отсутствует DIB-заголовок");
        return false;
    }
    const quint32 dibSize = readLe32(dibSizeBytes.constData());
    if (dibSize < 12 || static_cast<quint64>(14) + dibSize > static_cast<quint64>(file.size()) || dibSize > 4096)
    {
        setCorrupted(metadata, "Некорректный размер DIB-заголовка");
        return false;
    }

    if (!file.seek(14))
    {
        setCorrupted(metadata, "Не удалось прочитать DIB-заголовок");
        return false;
    }
    const QByteArray dib = file.read(std::min<quint32>(dibSize, 124));
    if (dib.size() < static_cast<int>(std::min<quint32>(dibSize, 40)))
    {
        setCorrupted(metadata, "Обрезан DIB-заголовок");
        return false;
    }

    quint16 bpp = 0;
    quint32 compression = 0;
    quint32 imageSize = 0;
    quint32 colorsUsed = 0;

    if (dibSize == 12)
    {
        metadata.width = readLe16(dib.constData() + 4);
        metadata.height = readLe16(dib.constData() + 6);
        const quint16 planes = readLe16(dib.constData() + 8);
        bpp = readLe16(dib.constData() + 10);
        if (planes != 1)
        {
            setCorrupted(metadata, "Некорректное число плоскостей BMP");
            return false;
        }
        metadata.compression = "BI_RGB";
        appendDetail(metadata, "BITMAPCOREHEADER");
    }
    else if (dibSize >= 40)
    {
        const qint32 width = readLeS32(dib.constData() + 4);
        const qint32 height = readLeS32(dib.constData() + 8);
        const quint16 planes = readLe16(dib.constData() + 12);
        bpp = readLe16(dib.constData() + 14);
        compression = readLe32(dib.constData() + 16);
        imageSize = readLe32(dib.constData() + 20);
        const qint32 xPpm = readLeS32(dib.constData() + 24);
        const qint32 yPpm = readLeS32(dib.constData() + 28);
        if (dib.size() >= 36)
            colorsUsed = readLe32(dib.constData() + 32);

        if (width <= 0 || height == 0 || planes != 1)
        {
            setCorrupted(metadata, "Некорректная геометрия BMP");
            return false;
        }

        metadata.width = width;
        metadata.height = std::abs(height);
        metadata.compression = bmpCompressionName(compression);
        if ((compression == 1 && bpp != 8) || (compression == 2 && bpp != 4) ||
            (height < 0 && compression != 0 && compression != 3 && compression != 6))
        {
            setCorrupted(metadata, "Несовместимые параметры глубины цвета и сжатия BMP");
            return false;
        }
        if (xPpm != 0 && yPpm != 0)
        {
            metadata.dpiX = std::abs(xPpm) * 0.0254;
            metadata.dpiY = std::abs(yPpm) * 0.0254;
        }
        appendDetail(metadata, QString("DIB header: %1 байт").arg(dibSize));
    }
    else
    {
        setCorrupted(metadata, "Неподдерживаемый DIB-заголовок");
        return false;
    }

    if (metadata.width <= 0 || metadata.height <= 0 || bpp == 0)
    {
        setCorrupted(metadata, "Некорректные параметры BMP");
        return false;
    }

    metadata.colorDepth = bpp;
    metadata.colorDepthText = QString("%1 бит").arg(bpp);

    if (static_cast<quint64>(pixelOffset) < static_cast<quint64>(14) + dibSize)
    {
        setCorrupted(metadata, "Пиксельные данные начинаются внутри DIB-заголовка");
        return false;
    }

    if (bpp <= 8)
    {
        const quint32 paletteSize = colorsUsed > 0 ? colorsUsed : (1u << bpp);
        const quint64 paletteEntrySize = dibSize == 12 ? 3ULL : 4ULL;
        const quint64 paletteEnd = 14ULL + dibSize + static_cast<quint64>(paletteSize) * paletteEntrySize;
        if (paletteEnd > pixelOffset)
        {
            setCorrupted(metadata, "Палитра BMP не помещается до массива пикселей");
            return false;
        }
        appendDetail(metadata, QString("Палитра: до %1 цветов").arg(paletteSize));
    }

    if (imageSize > 0 && static_cast<quint64>(pixelOffset) + imageSize > static_cast<quint64>(file.size()))
    {
        setCorrupted(metadata, "Размер пиксельных данных превышает размер файла");
        return false;
    }

    if (compression == 0)
    {
        const quint64 rowBits = static_cast<quint64>(metadata.width) * bpp;
        const quint64 rowBytes = ((rowBits + 31ULL) / 32ULL) * 4ULL;
        const quint64 minimumSize = static_cast<quint64>(pixelOffset) + rowBytes * metadata.height;
        if (minimumSize > static_cast<quint64>(file.size()))
        {
            setCorrupted(metadata, "BMP обрезан относительно несжатого размера строк");
            return false;
        }
    }

    return true;
}

bool skipGifSubBlocks(QFile& file)
{
    while (file.pos() < file.size())
    {
        char sizeByte{};
        if (file.read(&sizeByte, 1) != 1)
            return false;
        const int blockSize = static_cast<unsigned char>(sizeByte);
        if (blockSize == 0)
            return true;
        if (file.pos() + blockSize > file.size())
            return false;
        if (!file.seek(file.pos() + blockSize))
            return false;
    }
    return false;
}

bool parseGif(QFile& file, ImageMetadata& metadata)
{
    if (file.size() < 14 || !file.seek(0))
    {
        setCorrupted(metadata, "GIF слишком мал");
        return false;
    }

    const QByteArray header = file.read(13);
    if (header.size() != 13 || (header.left(6) != "GIF87a" && header.left(6) != "GIF89a"))
    {
        setCorrupted(metadata, "Некорректная GIF-сигнатура");
        return false;
    }

    metadata.width = readLe16(header.constData() + 6);
    metadata.height = readLe16(header.constData() + 8);
    const unsigned char packed = static_cast<unsigned char>(header[10]);
    const bool hasGlobalTable = (packed & 0x80) != 0;
    const int colorResolution = ((packed >> 4) & 0x07) + 1;
    int paletteBits = hasGlobalTable ? (packed & 0x07) + 1 : 0;

    if (metadata.width <= 0 || metadata.height <= 0)
    {
        setCorrupted(metadata, "Некорректная геометрия GIF");
        return false;
    }

    if (hasGlobalTable)
    {
        const qint64 tableSize = 3LL * (1LL << paletteBits);
        if (file.pos() + tableSize > file.size() || !file.seek(file.pos() + tableSize))
        {
            setCorrupted(metadata, "Обрезана глобальная палитра GIF");
            return false;
        }
    }

    bool imageFound = false;
    bool trailerFound = false;
    int lzwMinimum = -1;

    while (file.pos() < file.size())
    {
        char introducer{};
        if (file.read(&introducer, 1) != 1)
            break;

        const unsigned char value = static_cast<unsigned char>(introducer);
        if (value == 0x3B)
        {
            trailerFound = true;
            break;
        }

        if (value == 0x21)
        {
            char label{};
            if (file.read(&label, 1) != 1 || !skipGifSubBlocks(file))
            {
                setCorrupted(metadata, "Обрезан extension block GIF");
                return false;
            }
            continue;
        }

        if (value == 0x2C)
        {
            const QByteArray descriptor = file.read(9);
            if (descriptor.size() != 9)
            {
                setCorrupted(metadata, "Обрезан Image Descriptor GIF");
                return false;
            }

            const quint16 imageLeft = readLe16(descriptor.constData());
            const quint16 imageTop = readLe16(descriptor.constData() + 2);
            const quint16 imageWidth = readLe16(descriptor.constData() + 4);
            const quint16 imageHeight = readLe16(descriptor.constData() + 6);
            if (imageWidth == 0 || imageHeight == 0 ||
                static_cast<quint32>(imageLeft) + imageWidth > static_cast<quint32>(metadata.width) ||
                static_cast<quint32>(imageTop) + imageHeight > static_cast<quint32>(metadata.height))
            {
                setCorrupted(metadata, "Некорректная геометрия Image Descriptor GIF");
                return false;
            }

            const unsigned char imagePacked = static_cast<unsigned char>(descriptor[8]);
            if ((imagePacked & 0x80) != 0)
            {
                const int localBits = (imagePacked & 0x07) + 1;
                paletteBits = std::max(paletteBits, localBits);
                const qint64 tableSize = 3LL * (1LL << localBits);
                if (file.pos() + tableSize > file.size() || !file.seek(file.pos() + tableSize))
                {
                    setCorrupted(metadata, "Обрезана локальная палитра GIF");
                    return false;
                }
            }

            char lzw{};
            if (file.read(&lzw, 1) != 1)
            {
                setCorrupted(metadata, "Отсутствует LZW minimum code size");
                return false;
            }
            lzwMinimum = static_cast<unsigned char>(lzw);
            if (lzwMinimum < 2 || lzwMinimum > 8)
            {
                setCorrupted(metadata, "Некорректный LZW minimum code size GIF");
                return false;
            }
            if (!skipGifSubBlocks(file))
            {
                setCorrupted(metadata, "Обрезаны LZW-данные GIF");
                return false;
            }
            imageFound = true;
            continue;
        }

        setCorrupted(metadata, QString("Неизвестный блок GIF 0x%1").arg(value, 2, 16, QLatin1Char('0')));
        return false;
    }

    if (!imageFound)
    {
        setCorrupted(metadata, "В GIF отсутствует изображение");
        return false;
    }
    if (!trailerFound)
    {
        setCorrupted(metadata, "В GIF отсутствует trailer 3B");
        return false;
    }

    metadata.colorDepth = paletteBits > 0 ? paletteBits : colorResolution;
    metadata.colorDepthText = QString("%1 бит, индексированный").arg(metadata.colorDepth);
    metadata.compression = lzwMinimum >= 0
        ? QString("LZW (min code %1)").arg(lzwMinimum)
        : "LZW";
    appendDetail(metadata, QString("Color resolution: %1 бит").arg(colorResolution));
    if (paletteBits > 0)
        appendDetail(metadata, QString("Палитра: до %1 цветов").arg(1 << paletteBits));
    return true;
}

QString tiffCompressionName(quint32 value)
{
    switch (value)
    {
    case 1: return "Без сжатия";
    case 2: return "CCITT 1D";
    case 3: return "CCITT Group 3 Fax";
    case 4: return "CCITT Group 4 Fax";
    case 5: return "LZW";
    case 6: return "Old JPEG";
    case 7: return "JPEG";
    case 8: return "Deflate";
    case 32773: return "PackBits";
    case 32946: return "Deflate";
    default: return QString("TIFF compression %1").arg(value);
    }
}

QString photometricName(quint32 value)
{
    switch (value)
    {
    case 0: return "WhiteIsZero";
    case 1: return "BlackIsZero";
    case 2: return "RGB";
    case 3: return "Palette";
    case 4: return "Transparency mask";
    case 5: return "CMYK";
    case 6: return "YCbCr";
    case 8: return "CIELab";
    default: return QString::number(value);
    }
}

struct TiffEntry
{
    quint16 tag{};
    quint16 type{};
    quint32 count{};
    quint32 valueOffset{};
    qint64 entryOffset{};
};

class TiffReader
{
public:
    TiffReader(QFile& file, bool littleEndian)
        : file_(file), littleEndian_(littleEndian)
    {
    }

    bool readU16(qint64 offset, quint16& value)
    {
        QByteArray data;
        if (!read(offset, 2, data))
            return false;
        value = littleEndian_ ? readLe16(data.constData()) : readBe16(data.constData());
        return true;
    }

    bool readU32(qint64 offset, quint32& value)
    {
        QByteArray data;
        if (!read(offset, 4, data))
            return false;
        value = littleEndian_ ? readLe32(data.constData()) : readBe32(data.constData());
        return true;
    }

    bool readEntry(qint64 offset, TiffEntry& entry)
    {
        entry.entryOffset = offset;
        return readU16(offset, entry.tag) &&
               readU16(offset + 2, entry.type) &&
               readU32(offset + 4, entry.count) &&
               readU32(offset + 8, entry.valueOffset);
    }

    bool readUnsignedValues(const TiffEntry& entry, QVector<quint32>& values)
    {
        int elementSize = 0;
        if (entry.type == 1) elementSize = 1;
        if (entry.type == 3) elementSize = 2;
        if (entry.type == 4) elementSize = 4;
        if (elementSize == 0 || entry.count == 0 || entry.count > 1000000)
            return false;

        const quint64 totalBytes = static_cast<quint64>(elementSize) * entry.count;
        const qint64 dataOffset = totalBytes <= 4 ? entry.entryOffset + 8 : entry.valueOffset;
        if (dataOffset < 0 || static_cast<quint64>(dataOffset) + totalBytes > static_cast<quint64>(file_.size()))
            return false;

        values.clear();
        values.reserve(static_cast<int>(entry.count));
        for (quint32 i = 0; i < entry.count; ++i)
        {
            if (entry.type == 1)
            {
                QByteArray data;
                if (!read(dataOffset + i, 1, data))
                    return false;
                values.push_back(static_cast<unsigned char>(data[0]));
            }
            else if (entry.type == 3)
            {
                quint16 value{};
                if (!readU16(dataOffset + static_cast<qint64>(i) * 2, value))
                    return false;
                values.push_back(value);
            }
            else
            {
                quint32 value{};
                if (!readU32(dataOffset + static_cast<qint64>(i) * 4, value))
                    return false;
                values.push_back(value);
            }
        }
        return true;
    }

    bool readRational(const TiffEntry& entry, double& value)
    {
        if (entry.type != 5 || entry.count == 0)
            return false;
        const qint64 dataOffset = entry.valueOffset;
        quint32 numerator{};
        quint32 denominator{};
        if (!readU32(dataOffset, numerator) || !readU32(dataOffset + 4, denominator) || denominator == 0)
            return false;
        value = static_cast<double>(numerator) / denominator;
        return true;
    }

private:
    bool read(qint64 offset, qint64 count, QByteArray& data)
    {
        if (offset < 0 || count < 0 || offset + count > file_.size() || !file_.seek(offset))
            return false;
        data = file_.read(count);
        return data.size() == count;
    }

    QFile& file_;
    bool littleEndian_{};
};

bool validateTiffRanges(TiffReader& reader, const QHash<quint16, TiffEntry>& entries,
                        quint16 offsetsTag, quint16 countsTag, qint64 fileSize)
{
    if (!entries.contains(offsetsTag) || !entries.contains(countsTag))
        return true;

    QVector<quint32> offsets;
    QVector<quint32> counts;
    if (!reader.readUnsignedValues(entries[offsetsTag], offsets) ||
        !reader.readUnsignedValues(entries[countsTag], counts) || offsets.size() != counts.size())
        return false;

    for (int i = 0; i < offsets.size(); ++i)
    {
        const quint64 end = static_cast<quint64>(offsets[i]) + counts[i];
        if (end > static_cast<quint64>(fileSize))
            return false;
    }
    return true;
}

bool parseTiff(QFile& file, ImageMetadata& metadata)
{
    if (file.size() < 8 || !file.seek(0))
    {
        setCorrupted(metadata, "TIFF слишком мал");
        return false;
    }

    const QByteArray header = file.read(8);
    if (header.size() != 8)
    {
        setCorrupted(metadata, "Обрезан TIFF-заголовок");
        return false;
    }

    const bool little = header[0] == 'I' && header[1] == 'I';
    const bool big = header[0] == 'M' && header[1] == 'M';
    if (!little && !big)
    {
        setCorrupted(metadata, "Некорректный порядок байтов TIFF");
        return false;
    }

    const quint16 magic = little ? readLe16(header.constData() + 2) : readBe16(header.constData() + 2);
    if (magic != 42)
    {
        setCorrupted(metadata, magic == 43 ? "BigTIFF не поддерживается" : "Некорректное magic number TIFF");
        return false;
    }

    const quint32 ifdOffset = little ? readLe32(header.constData() + 4) : readBe32(header.constData() + 4);
    if (ifdOffset < 8 || ifdOffset + 2ULL > static_cast<quint64>(file.size()))
    {
        setCorrupted(metadata, "Некорректное смещение IFD");
        return false;
    }

    TiffReader reader(file, little);
    quint16 entryCount{};
    if (!reader.readU16(ifdOffset, entryCount))
    {
        setCorrupted(metadata, "Не удалось прочитать число IFD-тегов");
        return false;
    }

    const quint64 ifdEnd = static_cast<quint64>(ifdOffset) + 2ULL + static_cast<quint64>(entryCount) * 12ULL + 4ULL;
    if (ifdEnd > static_cast<quint64>(file.size()))
    {
        setCorrupted(metadata, "IFD выходит за границы файла");
        return false;
    }

    QHash<quint16, TiffEntry> entries;
    for (quint16 i = 0; i < entryCount; ++i)
    {
        TiffEntry entry;
        if (!reader.readEntry(static_cast<qint64>(ifdOffset) + 2 + static_cast<qint64>(i) * 12, entry))
        {
            setCorrupted(metadata, "Обрезан IFD-тег");
            return false;
        }
        if (!entries.contains(entry.tag))
            entries.insert(entry.tag, entry);
    }

    auto firstUnsigned = [&](quint16 tag, quint32 defaultValue, bool required, bool& ok) -> quint32 {
        if (!entries.contains(tag))
        {
            ok = !required;
            return defaultValue;
        }
        QVector<quint32> values;
        if (!reader.readUnsignedValues(entries[tag], values) || values.isEmpty())
        {
            ok = false;
            return defaultValue;
        }
        ok = true;
        return values.front();
    };

    bool okWidth = false;
    bool okHeight = false;
    bool okCompression = false;
    bool okSamples = false;
    bool okPhotometric = false;
    bool okUnit = false;

    metadata.width = static_cast<int>(firstUnsigned(256, 0, true, okWidth));
    metadata.height = static_cast<int>(firstUnsigned(257, 0, true, okHeight));
    const quint32 compression = firstUnsigned(259, 1, false, okCompression);
    const quint32 samplesPerPixel = firstUnsigned(277, 1, false, okSamples);
    const quint32 photometric = firstUnsigned(262, 0, false, okPhotometric);
    const quint32 resolutionUnit = firstUnsigned(296, 2, false, okUnit);

    if (!okWidth || !okHeight || metadata.width <= 0 || metadata.height <= 0)
    {
        setCorrupted(metadata, "В IFD отсутствуют корректные ImageWidth/ImageLength");
        return false;
    }

    QVector<quint32> bits;
    if (entries.contains(258))
    {
        if (!reader.readUnsignedValues(entries[258], bits) || bits.isEmpty())
        {
            setCorrupted(metadata, "Некорректный BitsPerSample");
            return false;
        }
    }
    else
    {
        bits.push_back(1);
    }

    quint64 totalBits = 0;
    if (bits.size() == 1 && samplesPerPixel > 1)
        totalBits = static_cast<quint64>(bits.front()) * samplesPerPixel;
    else
        for (quint32 value : bits)
            totalBits += value;

    if (totalBits > static_cast<quint64>(std::numeric_limits<int>::max()))
    {
        setCorrupted(metadata, "Слишком большая глубина цвета TIFF");
        return false;
    }

    metadata.colorDepth = static_cast<int>(totalBits);
    metadata.colorDepthText = QString("%1 бит").arg(metadata.colorDepth);
    metadata.compression = tiffCompressionName(compression);

    double xResolution = 0.0;
    double yResolution = 0.0;
    if (entries.contains(282))
        reader.readRational(entries[282], xResolution);
    if (entries.contains(283))
        reader.readRational(entries[283], yResolution);

    if (xResolution > 0.0 && yResolution > 0.0)
    {
        if (resolutionUnit == 2)
        {
            metadata.dpiX = xResolution;
            metadata.dpiY = yResolution;
        }
        else if (resolutionUnit == 3)
        {
            metadata.dpiX = xResolution * 2.54;
            metadata.dpiY = yResolution * 2.54;
        }
    }

    if (!validateTiffRanges(reader, entries, 273, 279, file.size()) ||
        !validateTiffRanges(reader, entries, 324, 325, file.size()))
    {
        setCorrupted(metadata, "Strip/Tile offsets или byte counts выходят за границы файла");
        return false;
    }

    appendDetail(metadata, QString("IFD-тегов: %1; SamplesPerPixel: %2; Photometric: %3")
        .arg(entryCount).arg(samplesPerPixel).arg(photometricName(photometric)));
    appendDetail(metadata, little ? "Byte order: little-endian" : "Byte order: big-endian");
    return true;
}

bool parsePcx(QFile& file, ImageMetadata& metadata)
{
    if (file.size() < 128 || !file.seek(0))
    {
        setCorrupted(metadata, "PCX меньше 128-байтового заголовка");
        return false;
    }

    const QByteArray header = file.read(128);
    if (header.size() != 128 || static_cast<unsigned char>(header[0]) != 0x0A)
    {
        setCorrupted(metadata, "Некорректная PCX-сигнатура");
        return false;
    }

    const int version = static_cast<unsigned char>(header[1]);
    const int encoding = static_cast<unsigned char>(header[2]);
    const int bitsPerPlane = static_cast<unsigned char>(header[3]);
    const quint16 xMin = readLe16(header.constData() + 4);
    const quint16 yMin = readLe16(header.constData() + 6);
    const quint16 xMax = readLe16(header.constData() + 8);
    const quint16 yMax = readLe16(header.constData() + 10);
    const quint16 hDpi = readLe16(header.constData() + 12);
    const quint16 vDpi = readLe16(header.constData() + 14);
    const int planes = static_cast<unsigned char>(header[65]);
    const quint16 bytesPerLine = readLe16(header.constData() + 66);

    if (version > 5 || encoding > 1 || bitsPerPlane <= 0 || planes <= 0 ||
        xMax < xMin || yMax < yMin || bytesPerLine == 0)
    {
        setCorrupted(metadata, "Некорректные поля заголовка PCX");
        return false;
    }

    metadata.width = static_cast<int>(xMax - xMin + 1);
    metadata.height = static_cast<int>(yMax - yMin + 1);
    metadata.dpiX = hDpi;
    metadata.dpiY = vDpi;
    metadata.colorDepth = bitsPerPlane * planes;
    metadata.colorDepthText = QString("%1 бит (%2 плоск. x %3 бит)")
        .arg(metadata.colorDepth).arg(planes).arg(bitsPerPlane);
    metadata.compression = encoding == 1 ? "PCX RLE" : "Без сжатия";

    if (encoding == 0)
    {
        const quint64 required = 128ULL + static_cast<quint64>(bytesPerLine) *
            static_cast<quint64>(planes) * static_cast<quint64>(metadata.height);
        if (required > static_cast<quint64>(file.size()))
        {
            setCorrupted(metadata, "PCX обрезан относительно BytesPerLine");
            return false;
        }
    }
    else if (file.size() <= 128)
    {
        setCorrupted(metadata, "В PCX отсутствуют RLE-данные");
        return false;
    }

    bool vgaPalette = false;
    if (file.size() >= 897 && file.seek(file.size() - 769))
    {
        char marker{};
        if (file.read(&marker, 1) == 1 && static_cast<unsigned char>(marker) == 0x0C)
            vgaPalette = true;
    }

    appendDetail(metadata, QString("Версия PCX: %1; BytesPerLine: %2").arg(version).arg(bytesPerLine));
    if (vgaPalette)
        appendDetail(metadata, "VGA-палитра: 256 цветов");
    return true;
}
}

ImageMetadata ImageParser::parseFile(const QString& filePath)
{
    ImageMetadata metadata;
    const QFileInfo info(filePath);
    metadata.filePath = info.absoluteFilePath();
    metadata.fileName = info.fileName();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        metadata.format = expectedFormatForSuffix(info.suffix());
        setCorrupted(metadata, "Не удалось открыть файл для чтения");
        return metadata;
    }

    const QByteArray header = file.peek(16);
    const QString detected = detectFormat(header);
    const QString expected = expectedFormatForSuffix(info.suffix());

    if (detected.isEmpty())
    {
        metadata.format = expected.isEmpty() ? "Неизвестно" : expected;
        setCorrupted(metadata, "Сигнатура не соответствует поддерживаемому графическому формату");
        return metadata;
    }

    metadata.format = detected;
    bool parsed = false;
    if (detected == "JPEG") parsed = parseJpeg(file, metadata);
    else if (detected == "PNG") parsed = parsePng(file, metadata);
    else if (detected == "BMP") parsed = parseBmp(file, metadata);
    else if (detected == "GIF") parsed = parseGif(file, metadata);
    else if (detected == "TIFF") parsed = parseTiff(file, metadata);
    else if (detected == "PCX") parsed = parsePcx(file, metadata);

    if (parsed)
        applyWindowsDefaults(metadata);

    if (parsed && !expected.isEmpty() && expected != detected)
        setWarning(metadata, QString("Расширение .%1 не соответствует сигнатуре %2").arg(info.suffix(), detected));

    return metadata;
}
