// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual form through the same standard Qt loader used by KWin.
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTest>
#include <QUiLoader>
#include <QWidget>
#include <memory>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFile file(QStringLiteral("src/contents/ui/config.ui"));
    if (!file.open(QFile::ReadOnly)) return 1;
    QUiLoader loader;
    std::unique_ptr<QWidget> form(loader.load(&file));
    if (!form) { qCritical() << loader.errorString(); return 1; }
    const auto fail = [](const char *message) { qCritical() << message; return 1; };
    auto *columns = form->findChild<QSpinBox *>("kcfg_gridColumns");
    auto *rows = form->findChild<QSpinBox *>("kcfg_gridRows");
    auto *subdivide = form->findChild<QCheckBox *>("kcfg_subdivideZones");
    auto *advanced = form->findChild<QPushButton *>("advancedToggle");
    auto *advancedOptions = form->findChild<QWidget *>("advancedOptions");
    auto *shortcuts = form->findChild<QLabel *>("shortcutsLink");
    if (!columns || !rows || !subdivide || !advanced || !advancedOptions || !shortcuts)
        return fail("Required controls are missing");
    form->show();
    QTest::qWait(20);
    auto preview = [&]() -> QTableWidget * {
        QTableWidget *visible = nullptr;
        for (auto *table : form->findChildren<QTableWidget *>()) {
            if (table->isVisible()) {
                if (visible) return nullptr;
                visible = table;
            }
        }
        return visible;
    };
    auto *grid = preview();
    if (!grid || grid->columnCount() != 4 || grid->rowCount() != 2)
        return fail("Default preview is not 4x2");
    if (advancedOptions->isVisible()) return fail("Advanced options start expanded");
    subdivide->setChecked(true);
    grid = preview();
    if (!grid || grid->columnCount() != 8 || grid->rowCount() != 4)
        return fail("Subdivision preview is not 8x4");
    columns->setValue(3);
    rows->setValue(1);
    grid = preview();
    if (!grid || grid->columnCount() != 6 || grid->rowCount() != 2)
        return fail("Preview did not follow changed controls");
    subdivide->setChecked(false);
    grid = preview();
    if (!grid || grid->columnCount() != 3 || grid->rowCount() != 1)
        return fail("Disabling subdivision did not restore the base grid");
    advanced->click();
    if (!advancedOptions->isVisible()) return fail("Advanced options did not expand");
    advanced->click();
    if (advancedOptions->isVisible()) return fail("Advanced options did not collapse");
    if (!shortcuts->openExternalLinks() || !shortcuts->text().contains("systemsettings://kcm_keys"))
        return fail("Shortcut link does not target the KDE shortcuts page");
    columns->setValue(4); rows->setValue(2);
    form->adjustSize();
    QTest::qWait(20);
    const QString screenshot = qEnvironmentVariable("KFANCYZONES_SETTINGS_SCREENSHOT");
    if (!screenshot.isEmpty()) form->grab().save(screenshot);
    qInfo() << "Settings form: PASS (4x2, 8x4 subdivision, changed grid, Advanced, shortcut link)";
    return 0;
}
