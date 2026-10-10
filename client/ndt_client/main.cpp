#include "NdtWindow.h"
#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    NdtWindow window;
    window.show();
    return app.exec();
}
