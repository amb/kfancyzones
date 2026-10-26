// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle(QStringLiteral("KZones integration target"));
    window.resize(600, 400);
    window.show();
    return app.exec();
}
