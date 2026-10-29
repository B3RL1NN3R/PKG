#include <QtTest>
#include "model/ImageBuffer.h"
#include "processing/Filters.h"
#include "processing/Morphology.h"
#include <random>

class FiltersTests : public QObject {
    Q_OBJECT

private:
    ImageBuffer makeUniform(int w, int h, uint8_t value) {
        ImageBuffer img(w, h, 3);
        for (int i = 0; i < w * h * 3; ++i) {
            img.data()[i] = value;
        }
        return img;
    }

private slots:

    // ===== ImageBuffer =====

    void testImageBufferCreation() {
        ImageBuffer img(10, 20, 3);
        QCOMPARE(img.width(), 10);
        QCOMPARE(img.height(), 20);
        QCOMPARE(img.channels(), 3);
    }

    void testImageBufferGetSet() {
        ImageBuffer img(5, 5, 3);
        img.set(2, 3, 0, 100);
        img.set(2, 3, 1, 150);
        img.set(2, 3, 2, 200);

        QCOMPARE(int(img.get(2, 3, 0, EdgeMode::Clamp)), 100);
        QCOMPARE(int(img.get(2, 3, 1, EdgeMode::Clamp)), 150);
        QCOMPARE(int(img.get(2, 3, 2, EdgeMode::Clamp)), 200);
    }

    void testImageBufferOutOfBounds() {
        ImageBuffer img(5, 5, 3);
        img.set(2, 3, 0, 42);
        img.set(-1, 0, 0, 99);
        QCOMPARE(int(img.get(-1, 0, 0, EdgeMode::Clamp)), 0);
    }

    void testEdgeModeClamp() {
        ImageBuffer img(3, 3, 1);
        for (int i = 0; i < 9; ++i) img.data()[i] = uint8_t(i * 20);
        QCOMPARE(int(img.get(-5, 0, 0, EdgeMode::Clamp)), int(img.get(0, 0, 0, EdgeMode::Clamp)));
        QCOMPARE(int(img.get(10, 0, 0, EdgeMode::Clamp)), int(img.get(2, 0, 0, EdgeMode::Clamp)));
    }

    void testEdgeModeReflect() {
        ImageBuffer img(3, 3, 1);
        for (int i = 0; i < 9; ++i) img.data()[i] = uint8_t(i * 20);
        QCOMPARE(int(img.get(-1, 0, 0, EdgeMode::Reflect)), int(img.get(1, 0, 0, EdgeMode::Clamp)));
        QCOMPARE(int(img.get(-2, 0, 0, EdgeMode::Reflect)), int(img.get(2, 0, 0, EdgeMode::Clamp)));
    }

    void testToQImageRoundTrip() {
        ImageBuffer img(4, 4, 3);
        img.set(1, 1, 0, 10);
        img.set(1, 1, 1, 20);
        img.set(1, 1, 2, 30);

        QImage qimg = img.toQImage();
        QCOMPARE(qimg.width(), 4);
        QCOMPARE(qimg.height(), 4);
        QCOMPARE(qimg.format(), QImage::Format_RGB32);

        QRgb pixel = qimg.pixel(1, 1);
        QCOMPARE(int(qRed(pixel)), 10);
        QCOMPARE(int(qGreen(pixel)), 20);
        QCOMPARE(int(qBlue(pixel)), 30);
    }

    // ===== Box Filter =====

    void testBoxFilterUniform() {
        ImageBuffer src = makeUniform(20, 20, 128);
        ImageBuffer out = Filters::boxFilter(src, 2, EdgeMode::Clamp);
        for (int y = 0; y < out.height(); ++y) {
            for (int x = 0; x < out.width(); ++x) {
                QCOMPARE(int(out.get(x, y, 0, EdgeMode::Clamp)), 128);
                QCOMPARE(int(out.get(x, y, 1, EdgeMode::Clamp)), 128);
                QCOMPARE(int(out.get(x, y, 2, EdgeMode::Clamp)), 128);
            }
        }
    }

    void testBoxFilterSmooths() {
        ImageBuffer src(11, 11, 1);
        for (int i = 0; i < 11 * 11; ++i) src.data()[i] = 0;
        src.set(5, 5, 0, 255);

        ImageBuffer out = Filters::boxFilter(src, 2, EdgeMode::Clamp);
        QVERIFY(out.get(5, 5, 0, EdgeMode::Clamp) < 255);
        QVERIFY(out.get(5, 6, 0, EdgeMode::Clamp) > 0);
    }

    // ===== Gaussian Filter =====

    void testGaussianFilterUniform() {
        ImageBuffer src = makeUniform(20, 20, 100);
        ImageBuffer out = Filters::gaussianFilter(src, 3, EdgeMode::Reflect);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QCOMPARE(int(out.data()[i]), 100);
        }
    }

    void testGaussianFilterSmooths() {
        ImageBuffer src(11, 11, 1);
        for (int i = 0; i < 11 * 11; ++i) src.data()[i] = 0;
        src.set(5, 5, 0, 255);

        ImageBuffer out = Filters::gaussianFilter(src, 2, EdgeMode::Reflect);
        QVERIFY(out.get(5, 5, 0, EdgeMode::Clamp) < 255);
        QVERIFY(out.get(5, 6, 0, EdgeMode::Clamp) > 0);
    }

    // ===== Morphology =====

    void testErosionDarkens() {
        ImageBuffer src = makeUniform(10, 10, 200);
        src.set(5, 5, 0, 50);
        src.set(5, 5, 1, 50);
        src.set(5, 5, 2, 50);

        ImageBuffer out = Morphology::erode(src, 1, EdgeMode::Clamp);
        QCOMPARE(int(out.get(5, 5, 0, EdgeMode::Clamp)), 50);
    }

    void testDilationBrightens() {
        ImageBuffer src = makeUniform(10, 10, 50);
        src.set(5, 5, 0, 200);
        src.set(5, 5, 1, 200);
        src.set(5, 5, 2, 200);

        ImageBuffer out = Morphology::dilate(src, 1, EdgeMode::Clamp);
        QCOMPARE(int(out.get(5, 5, 0, EdgeMode::Clamp)), 200);
        QVERIFY(out.get(5, 6, 0, EdgeMode::Clamp) >= 50);
    }

    void testMorphologicalGradient() {
        ImageBuffer src = makeUniform(10, 10, 150);
        ImageBuffer out = Morphology::apply(src, MorphOp::Gradient, 1, EdgeMode::Clamp);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QCOMPARE(int(out.data()[i]), 0);
        }
    }

    // Top-Hat на однородном изображении = 0
    void testTopHatOnUniform() {
        ImageBuffer src = makeUniform(10, 10, 100);
        ImageBuffer out = Morphology::apply(src, MorphOp::TopHat, 1, EdgeMode::Clamp);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QCOMPARE(int(out.data()[i]), 0);
        }
    }

    // Black-Hat на однородном изображении = 0
    void testBlackHatOnUniform() {
        ImageBuffer src = makeUniform(10, 10, 100);
        ImageBuffer out = Morphology::apply(src, MorphOp::BlackHat, 1, EdgeMode::Clamp);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QCOMPARE(int(out.data()[i]), 0);
        }
    }

    void testTopHatNonNegative() {
        std::mt19937 rng(42); // фиксированный seed для воспроизводимости
        std::uniform_int_distribution<int> dist(0, 255);

        ImageBuffer src(10, 10, 3);
        for (int i = 0; i < 10 * 10 * 3; ++i) {
            src.data()[i] = static_cast<uint8_t>(dist(rng));
        }
        ImageBuffer out = Morphology::apply(src, MorphOp::TopHat, 1, EdgeMode::Clamp);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QVERIFY(int(out.data()[i]) >= 0);
        }
    }

    void testBlackHatNonNegative() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, 255);

        ImageBuffer src(10, 10, 3);
        for (int i = 0; i < 10 * 10 * 3; ++i) {
            src.data()[i] = static_cast<uint8_t>(dist(rng));
        }
        ImageBuffer out = Morphology::apply(src, MorphOp::BlackHat, 1, EdgeMode::Clamp);
        for (int i = 0; i < out.width() * out.height() * 3; ++i) {
            QVERIFY(int(out.data()[i]) >= 0);
        }
    }
    void testColorPreserved() {
        ImageBuffer src(10, 10, 3);
        for (int i = 0; i < 10 * 10 * 3; ++i) src.data()[i] = 0;
        for (int y = 3; y < 7; ++y) {
            for (int x = 3; x < 7; ++x) {
                src.set(x, y, 0, 255);
                src.set(x, y, 1, 0);
                src.set(x, y, 2, 0);
            }
        }

        ImageBuffer out = Filters::boxFilter(src, 1, EdgeMode::Clamp);
        QCOMPARE(int(out.get(5, 5, 1, EdgeMode::Clamp)), 0);
        QCOMPARE(int(out.get(5, 5, 2, EdgeMode::Clamp)), 0);
        QVERIFY(out.get(5, 5, 0, EdgeMode::Clamp) > 0);
    }

    void testDifferentRadius() {
        ImageBuffer src = makeUniform(20, 20, 128);
        ImageBuffer out1 = Filters::boxFilter(src, 1, EdgeMode::Clamp);
        ImageBuffer out3 = Filters::boxFilter(src, 3, EdgeMode::Clamp);
        for (int i = 0; i < out1.width() * out1.height() * 3; ++i) {
            QCOMPARE(out1.data()[i], out3.data()[i]);
        }
    }
};

QTEST_MAIN(FiltersTests)
#include "FiltersTests.moc"