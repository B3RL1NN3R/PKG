#include "ImageBuffer.h"
#include <algorithm>
#include <QColor>

ImageBuffer::ImageBuffer(int width, int height, int channels)
    : m_width(width), m_height(height), m_channels(channels > 0 ? channels : 3)
{
    m_data.resize(width * height * m_channels);
    m_data.fill(0);
}

ImageBuffer::ImageBuffer(const QImage& img)
{
    QImage converted = img.convertToFormat(QImage::Format_RGB32);
    m_width = converted.width();
    m_height = converted.height();
    m_channels = 3;
    m_data.resize(m_width * m_height * 3);

    for (int y = 0; y < m_height; ++y) {
        const QRgb* src = reinterpret_cast<const QRgb*>(converted.constScanLine(y));
        uint8_t* dst = reinterpret_cast<uint8_t*>(m_data.data()) + y * m_width * 3;
        for (int x = 0; x < m_width; ++x) {
            dst[x * 3 + 0] = qRed(src[x]);
            dst[x * 3 + 1] = qGreen(src[x]);
            dst[x * 3 + 2] = qBlue(src[x]);
        }
    }
}

int ImageBuffer::clampCoord(int v, int maxV) const {
    return std::max(0, std::min(v, maxV - 1));
}

int ImageBuffer::reflectCoord(int v, int maxV) const {
    if (maxV <= 1) return 0;
    v = std::abs(v);
    int period = 2 * (maxV - 1);
    v %= period;
    if (v >= maxV) v = period - v;
    return v;
}

uint8_t ImageBuffer::get(int x, int y, int c, EdgeMode mode) const {
    int ix = (mode == EdgeMode::Clamp) ? clampCoord(x, m_width)  : reflectCoord(x, m_width);
    int iy = (mode == EdgeMode::Clamp) ? clampCoord(y, m_height) : reflectCoord(y, m_height);
    int ic = std::max(0, std::min(c, m_channels - 1));
    return static_cast<uint8_t>(m_data.constData()[(iy * m_width + ix) * m_channels + ic]);
}

void ImageBuffer::set(int x, int y, int c, uint8_t value) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || c < 0 || c >= m_channels)
        return;
    m_data.data()[(y * m_width + x) * m_channels + c] = value;
}

QImage ImageBuffer::toQImage() const {
    QImage img(m_width, m_height, QImage::Format_RGB32);
    for (int y = 0; y < m_height; ++y) {
        QRgb* dst = reinterpret_cast<QRgb*>(img.scanLine(y));
        const uint8_t* src = reinterpret_cast<const uint8_t*>(m_data.constData()) + y * m_width * 3;
        for (int x = 0; x < m_width; ++x) {
            dst[x] = qRgb(src[x * 3], src[x * 3 + 1], src[x * 3 + 2]);
        }
    }
    return img;
}