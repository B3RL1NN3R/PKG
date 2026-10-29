#include "view/MainWindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("ImageProcessor");
    application.setApplicationVersion(QStringLiteral(IMAGEPROCESSOR_VERSION));
    application.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    return application.exec();
}