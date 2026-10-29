#pragma once
#include <QImage>
#include <cstdint>

enum class EdgeMode { Clamp, Reflect };

class ImageBuffer {
public:
    ImageBuffer() = default;
    ImageBuffer(int width, int height, int channels);
    explicit ImageBuffer(const QImage& img);

    int width() const { return m_width; }
    int height() const { return m_height; }
    int channels() const { return m_channels; }
    const uint8_t* data() const { return reinterpret_cast<const uint8_t*>(m_data.constData()); }
    uint8_t* data() { return reinterpret_cast<uint8_t*>(m_data.data()); }

    uint8_t get(int x, int y, int c, EdgeMode mode) const;
    void set(int x, int y, int c, uint8_t value);

    QImage toQImage() const;

private:
    int m_width = 0;
    int m_height = 0;
    int m_channels = 3;
    QByteArray m_data;

    int clampCoord(int v, int maxV) const;
    int reflectCoord(int v, int maxV) const;
};