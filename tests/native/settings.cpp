// SPDX-License-Identifier: GPL-3.0-or-later
// Load the actual settings controller with a private configuration directory.
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <KCModule>
#include <KConfig>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <memory>

int main(int argc, char **argv)
{
    QTemporaryDir temporary;
    const QString configPath = temporary.path() + QStringLiteral("/config");
    QDir().mkpath(configPath);
    qputenv("XDG_CONFIG_HOME", configPath.toUtf8());
    QApplication app(argc, argv);
    const auto fail = [](const char *message) { qCritical() << message; return 1; };
    const QString contents = temporary.path() + QStringLiteral("/package/contents");
    for (const auto *directory : {"code", "config"}) QDir().mkpath(contents + '/' + directory);
    for (const auto *relative : {"code/configuration.js", "config/main.xml"}) {
        if (!QFile::copy(QStringLiteral("src/contents/") + relative, contents + '/' + relative))
            return fail("Could not stage settings files");
    }
    const QString pluginPath = contents + QStringLiteral("/config/libkfancyzonesconfig.so");
    if (!QFile::copy(QStringLiteral(KFANCYZONES_SETTINGS_PLUGIN), pluginPath)) return fail("Could not stage plugin");
    QFile metadataFile(QStringLiteral("src/metadata.json"));
    if (!metadataFile.open(QFile::ReadOnly)) return fail("Could not read metadata");
    const KPluginMetaData metadata(QJsonDocument::fromJson(metadataFile.readAll()).object(), pluginPath);
    const auto result = KPluginFactory::instantiatePlugin<KCModule>(metadata, nullptr);
    if (!result) { qCritical() << result.errorString; return 1; }
    std::unique_ptr<KCModule> module(result.plugin);
    module->load();
    QWidget *form = module->widget();
    auto *columns = form->findChild<QSpinBox *>("kcfg_gridColumns");
    auto *rows = form->findChild<QSpinBox *>("kcfg_gridRows");
    auto *subdivide = form->findChild<QCheckBox *>("kcfg_subdivideZones");
    auto *custom = form->findChild<QCheckBox *>("kcfg_useCustomLayout");
    auto *json = form->findChild<QPlainTextEdit *>("kcfg_layoutsJson");
    auto *error = form->findChild<QLabel *>("customLayoutError");
    auto *tabs = form->findChild<QTabWidget *>("settingsTabs");
    auto *advanced = form->findChild<QWidget *>("advancedOptions");
    auto *shortcuts = form->findChild<QLabel *>("shortcutsLink");
    auto *grid = form->findChild<QTableWidget *>("layoutPreview");
    if (!columns || !rows || !subdivide || !custom || !json || !error || !tabs || !advanced || !shortcuts || !grid)
        return fail("Required controls are missing");
    if (tabs->count() != 2 || tabs->tabText(0) != "Layout" || tabs->tabText(1) != "Advanced")
        return fail("Settings must have Layout and Advanced tabs");
    form->show();
    QTest::qWait(20);
    if (grid->columnCount() != 4 || grid->rowCount() != 2) return fail("Default preview is not 4x2");
    if (!grid->horizontalHeader()->isHidden() || !grid->verticalHeader()->isHidden())
        return fail("Default preview exposes numbered table headers");
    if (tabs->currentIndex() != 0 || advanced->isVisible()) return fail("Settings must start on Layout");
    subdivide->setChecked(true);
    if (grid->columnCount() != 8 || grid->rowCount() != 4) return fail("Subdivision preview is not 8x4");
    if (!grid->horizontalHeader()->isHidden() || !grid->verticalHeader()->isHidden())
        return fail("Subdivision exposes numbered table headers");
    columns->setValue(3); rows->setValue(1);
    if (grid->columnCount() != 6 || grid->rowCount() != 2) return fail("Preview does not follow grid controls");
    columns->setValue(4); rows->setValue(2);
    tabs->setCurrentIndex(1);
    if (!advanced->isVisible() || columns->isVisible()) return fail("Advanced is not a separate tab");
    if (custom->isChecked() || json->isEnabled() || error->isVisible()) return fail("Custom mode defaults are wrong");
    custom->setChecked(true);
    if (!json->isEnabled() || columns->isEnabled() || !error->isVisible())
        return fail("Empty enabled custom layout must be invalid and disable grid controls");
    if (!error->text().contains("default 4×2") || !error->styleSheet().contains("#da4453"))
        return fail("Invalid JSON must show a red warning with the 4x2 fallback");
    json->setPlainText(QStringLiteral("{broken"));
    if (!error->isVisible()) return fail("Invalid JSON did not show an error");
    const QString screenshot = qEnvironmentVariable("KFANCYZONES_SETTINGS_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        form->adjustSize(); QTest::qWait(20);
        form->grab().save(screenshot + QStringLiteral("-invalid.png"));
    }
    json->setPlainText(QStringLiteral("[{\"name\":\"Custom\",\"zones\":[{\"x\":0,\"y\":0,\"width\":100,\"height\":100}]}]"));
    if (error->isVisible()) return fail("Valid custom JSON did not clear the warning");
    module->save();
    KConfig saved(QStringLiteral("kwinrc"), KConfig::SimpleConfig);
    KConfigGroup group(&saved, QStringLiteral("Script-kfancyzones"));
    if (!group.readEntry("useCustomLayout", false) || !group.readEntry("subdivideZones", false))
        return fail("Apply did not persist custom mode and subdivision");
    custom->setChecked(false);
    json->setPlainText(QStringLiteral("{broken"));
    if (error->isVisible() || !columns->isEnabled() || json->isEnabled())
        return fail("Disabling custom mode did not restore the grid and hide validation");
    module->save(); saved.reparseConfiguration();
    if (group.readEntry("useCustomLayout", false)) return fail("Custom mode off was not persisted");
    module->defaults();
    if (custom->isChecked() || subdivide->isChecked()) return fail("Defaults did not restore base grid mode");
    module->load();
    if (custom->isChecked() || !subdivide->isChecked() || json->toPlainText() != "{broken")
        return fail("Reload did not preserve the saved grid and disabled JSON");
    tabs->setCurrentIndex(0);
    if (advanced->isVisible() || !columns->isVisible()) return fail("Could not return to Layout");
    if (!shortcuts->openExternalLinks() || !shortcuts->text().contains("systemsettings://kcm_keys"))
        return fail("Shortcut link is wrong");
    if (!screenshot.isEmpty()) { form->adjustSize(); QTest::qWait(20); form->grab().save(screenshot); }
    qInfo() << "Settings controller: PASS (grid, subdivision, tabs, live validation, custom toggle, Apply, defaults, reload)";
    return 0;
}
