#include "view/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("ImageMetadataReader");
    application.setApplicationVersion(QStringLiteral(IMAGEMETADATAREADER_VERSION));
    application.setOrganizationName("BSU");
    application.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    return application.exec();
}
