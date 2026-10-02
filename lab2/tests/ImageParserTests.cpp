#include "parser/ImageParser.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
void appendLe16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendLe32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 24) & 0xFF));
}

void appendBe16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>(value & 0xFF));
}

void appendBe32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>((value >> 24) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>(value & 0xFF));
}

void writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), static_cast<qint64>(data.size()));
}

QByteArray makeBmp()
{
    QByteArray data;
    data.append("BM", 2);
    appendLe32(data, 58);
    appendLe16(data, 0);
    appendLe16(data, 0);
    appendLe32(data, 54);
    appendLe32(data, 40);
    appendLe32(data, 1);
    appendLe32(data, 1);
    appendLe16(data, 1);
    appendLe16(data, 24);
    appendLe32(data, 0);
    appendLe32(data, 4);
    appendLe32(data, 3780);
    appendLe32(data, 3780);
    appendLe32(data, 0);
    appendLe32(data, 0);
    data.append(QByteArray(4, '\0'));
    return data;
}

void appendPngChunk(QByteArray& data, const QByteArray& type, const QByteArray& payload)
{
    appendBe32(data, static_cast<quint32>(payload.size()));
    data.append(type);
    data.append(payload);
    appendBe32(data, 0);
}

QByteArray makePng(bool withIend = true)
{
    QByteArray data("\x89PNG\r\n\x1a\n", 8);
    QByteArray ihdr;
    appendBe32(ihdr, 3);
    appendBe32(ihdr, 2);
    ihdr.append(static_cast<char>(8));
    ihdr.append(static_cast<char>(2));
    ihdr.append(static_cast<char>(0));
    ihdr.append(static_cast<char>(0));
    ihdr.append(static_cast<char>(0));
    appendPngChunk(data, "IHDR", ihdr);

    QByteArray phys;
    appendBe32(phys, 3780);
    appendBe32(phys, 3780);
    phys.append(static_cast<char>(1));
    appendPngChunk(data, "pHYs", phys);
    appendPngChunk(data, "IDAT", QByteArray());
    if (withIend)
        appendPngChunk(data, "IEND", QByteArray());
    return data;
}

QByteArray makeJpeg(bool withEoi = true)
{
    QByteArray data("\xFF\xD8", 2);
    data.append("\xFF\xE0", 2);
    appendBe16(data, 16);
    data.append("JFIF\0", 5);
    data.append(static_cast<char>(1));
    data.append(static_cast<char>(1));
    data.append(static_cast<char>(1));
    appendBe16(data, 72);
    appendBe16(data, 72);
    data.append(static_cast<char>(0));
    data.append(static_cast<char>(0));

    data.append("\xFF\xC0", 2);
    appendBe16(data, 17);
    data.append(static_cast<char>(8));
    appendBe16(data, 2);
    appendBe16(data, 3);
    data.append(static_cast<char>(3));
    data.append("\x01\x11\x00", 3);
    data.append("\x02\x11\x00", 3);
    data.append("\x03\x11\x00", 3);
    if (withEoi)
        data.append("\xFF\xD9", 2);
    return data;
}

QByteArray makeGif()
{
    QByteArray data("GIF89a", 6);
    appendLe16(data, 1);
    appendLe16(data, 1);
    data.append(static_cast<char>(0x80));
    data.append(static_cast<char>(0));
    data.append(static_cast<char>(0));
    data.append("\x00\x00\x00\xFF\xFF\xFF", 6);
    data.append(static_cast<char>(0x2C));
    appendLe16(data, 0);
    appendLe16(data, 0);
    appendLe16(data, 1);
    appendLe16(data, 1);
    data.append(static_cast<char>(0));
    data.append(static_cast<char>(2));
    data.append(static_cast<char>(2));
    data.append("\x4C\x01", 2);
    data.append(static_cast<char>(0));
    data.append(static_cast<char>(0x3B));
    return data;
}

QByteArray makePcx()
{
    QByteArray data(128, '\0');
    data[0] = static_cast<char>(0x0A);
    data[1] = static_cast<char>(5);
    data[2] = static_cast<char>(0);
    data[3] = static_cast<char>(8);
    data[8] = static_cast<char>(0);
    data[9] = static_cast<char>(0);
    data[10] = static_cast<char>(0);
    data[11] = static_cast<char>(0);
    data[12] = static_cast<char>(72);
    data[13] = static_cast<char>(0);
    data[14] = static_cast<char>(72);
    data[15] = static_cast<char>(0);
    data[65] = static_cast<char>(1);
    data[66] = static_cast<char>(1);
    data[67] = static_cast<char>(0);
    data.append(static_cast<char>(0));
    return data;
}

void appendTiffEntry(QByteArray& data, quint16 tag, quint16 type, quint32 count, quint32 value)
{
    appendLe16(data, tag);
    appendLe16(data, type);
    appendLe32(data, count);
    appendLe32(data, value);
}

QByteArray makeTiff()
{
    QByteArray data;
    data.append("II", 2);
    appendLe16(data, 42);
    appendLe32(data, 8);
    appendLe16(data, 8);
    appendTiffEntry(data, 256, 4, 1, 3);
    appendTiffEntry(data, 257, 4, 1, 2);
    appendTiffEntry(data, 258, 3, 3, 110);
    appendTiffEntry(data, 259, 3, 1, 1);
    appendTiffEntry(data, 262, 3, 1, 2);
    appendTiffEntry(data, 277, 3, 1, 3);
    appendTiffEntry(data, 282, 5, 1, 116);
    appendTiffEntry(data, 283, 5, 1, 124);
    appendLe32(data, 0);
    appendLe16(data, 8);
    appendLe16(data, 8);
    appendLe16(data, 8);
    appendLe32(data, 72);
    appendLe32(data, 1);
    appendLe32(data, 72);
    appendLe32(data, 1);
    return data;
}
}

class ImageParserTests : public QObject
{
    Q_OBJECT

private slots:
    void parsesSupportedFormats()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        writeFile(dir.filePath("sample.bmp"), makeBmp());
        writeFile(dir.filePath("sample.png"), makePng());
        writeFile(dir.filePath("sample.jpg"), makeJpeg());
        writeFile(dir.filePath("sample.gif"), makeGif());
        writeFile(dir.filePath("sample.pcx"), makePcx());
        writeFile(dir.filePath("sample.tif"), makeTiff());

        const ImageMetadata bmp = ImageParser::parseFile(dir.filePath("sample.bmp"));
        QCOMPARE(static_cast<int>(bmp.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(bmp.width, 1);
        QCOMPARE(bmp.height, 1);
        QCOMPARE(bmp.colorDepth, 24);
        QVERIFY(bmp.compression.contains("BI_RGB"));

        const ImageMetadata png = ImageParser::parseFile(dir.filePath("sample.png"));
        QCOMPARE(static_cast<int>(png.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(png.width, 3);
        QCOMPARE(png.height, 2);
        QCOMPARE(png.colorDepth, 24);
        QVERIFY(qAbs(png.dpiX - 96.012) < 0.1);

        const ImageMetadata jpeg = ImageParser::parseFile(dir.filePath("sample.jpg"));
        QCOMPARE(static_cast<int>(jpeg.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(jpeg.width, 3);
        QCOMPARE(jpeg.height, 2);
        QCOMPARE(jpeg.colorDepth, 24);
        QCOMPARE(qRound(jpeg.dpiX), 72);

        const ImageMetadata gif = ImageParser::parseFile(dir.filePath("sample.gif"));
        QCOMPARE(static_cast<int>(gif.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(gif.width, 1);
        QCOMPARE(gif.height, 1);
        QVERIFY(gif.compression.contains("LZW"));

        const ImageMetadata pcx = ImageParser::parseFile(dir.filePath("sample.pcx"));
        QCOMPARE(static_cast<int>(pcx.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(pcx.width, 1);
        QCOMPARE(pcx.height, 1);
        QCOMPARE(pcx.colorDepth, 8);

        const ImageMetadata tiff = ImageParser::parseFile(dir.filePath("sample.tif"));
        QCOMPARE(static_cast<int>(tiff.state), static_cast<int>(MetadataState::Ok));
        QCOMPARE(tiff.width, 3);
        QCOMPARE(tiff.height, 2);
        QCOMPARE(tiff.colorDepth, 24);
        QCOMPARE(qRound(tiff.dpiX), 72);
    }

    void detectsDamageAndExtensionMismatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        writeFile(dir.filePath("broken.png"), makePng(false));
        writeFile(dir.filePath("broken.jpg"), makeJpeg(false));
        writeFile(dir.filePath("renamed.jpg"), makePng());
        writeFile(dir.filePath("text.jpg"), QByteArray("not an image"));

        QCOMPARE(static_cast<int>(ImageParser::parseFile(dir.filePath("broken.png")).state), static_cast<int>(MetadataState::Corrupted));
        QCOMPARE(static_cast<int>(ImageParser::parseFile(dir.filePath("broken.jpg")).state), static_cast<int>(MetadataState::Corrupted));

        const ImageMetadata renamed = ImageParser::parseFile(dir.filePath("renamed.jpg"));
        QCOMPARE(static_cast<int>(renamed.state), static_cast<int>(MetadataState::Warning));
        QCOMPARE(renamed.format, QString("PNG"));

        QCOMPARE(static_cast<int>(ImageParser::parseFile(dir.filePath("text.jpg")).state), static_cast<int>(MetadataState::Corrupted));
    }
};

QTEST_APPLESS_MAIN(ImageParserTests)
#include "ImageParserTests.moc"
