// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QWidget>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QWidget neighbor;
    neighbor.setWindowTitle(QStringLiteral("KFancyZones integration neighbor"));
    neighbor.resize(400, 300);
    neighbor.show();
    QWidget window;
    window.setWindowTitle(QStringLiteral("KFancyZones integration target"));
    window.resize(600, 400);
    window.show();
    return app.exec();
}
