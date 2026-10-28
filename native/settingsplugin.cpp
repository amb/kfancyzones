// SPDX-License-Identifier: GPL-3.0-or-later
#include <KCModule>
#include <KConfigDialogManager>
#include <KConfigGroup>
#include <KConfigLoader>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJSEngine>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QUiLoader>
#include <QVBoxLayout>

class KFancyZonesConfig : public KCModule
{
    Q_OBJECT
public:
    KFancyZonesConfig(QObject *parent, const KPluginMetaData &metaData)
        : KCModule(qobject_cast<QWidget *>(parent), metaData)
        , m_config(KSharedConfig::openConfig(QStringLiteral("kwinrc")))
    {
        // The controller lives in the script's contents/config directory.
        const QDir contents(QFileInfo(metaData.fileName()).absolutePath() + QStringLiteral("/.."));
        QFile source(contents.filePath(QStringLiteral("code/configuration.js")));
        if (source.open(QFile::ReadOnly)) m_engine.evaluate(QString::fromUtf8(source.readAll()));
        m_load = m_engine.globalObject().property(QStringLiteral("load"));
        QFile ui(contents.filePath(QStringLiteral("ui/config.ui")));
        QUiLoader uiLoader;
        if (ui.open(QFile::ReadOnly)) m_form = uiLoader.load(&ui, widget());
        auto *container = new QVBoxLayout(widget());
        if (!m_form) {
            container->addWidget(new QLabel(uiLoader.errorString(), widget()));
            return;
        }
        container->addWidget(m_form);
        m_columns = m_form->findChild<QSpinBox *>("kcfg_gridColumns");
        m_rows = m_form->findChild<QSpinBox *>("kcfg_gridRows");
        m_gap = m_form->findChild<QSpinBox *>("kcfg_gridGap");
        m_subdivide = m_form->findChild<QCheckBox *>("kcfg_subdivideZones");
        m_custom = m_form->findChild<QCheckBox *>("kcfg_useCustomLayout");
        m_json = m_form->findChild<QPlainTextEdit *>("kcfg_layoutsJson");
        m_error = m_form->findChild<QLabel *>("customLayoutError");
        m_preview = m_form->findChild<QTableWidget *>("layoutPreview");
        m_preview->horizontalHeader()->hide();
        m_preview->verticalHeader()->hide();
        m_preview->horizontalHeader()->setMinimumSectionSize(1);
        m_preview->verticalHeader()->setMinimumSectionSize(1);
        m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_preview->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (auto *spin : {m_columns, m_rows}) {
            connect(spin, &QSpinBox::valueChanged, this, &KFancyZonesConfig::refresh);
        }
        connect(m_subdivide, &QCheckBox::toggled, this, &KFancyZonesConfig::refresh);
        connect(m_custom, &QCheckBox::toggled, this, &KFancyZonesConfig::refresh);
        connect(m_json, &QPlainTextEdit::textChanged, this, &KFancyZonesConfig::refresh);
        m_config->reparseConfiguration();
        QFile schema(contents.filePath(QStringLiteral("config/main.xml")));
        auto *loader = new KConfigLoader(KConfigGroup(m_config, QStringLiteral("Script-kfancyzones")), &schema, this);
        addConfig(loader, m_form);
        refresh();
    }

    void load() override { KCModule::load(); refresh(); }
    void defaults() override { KCModule::defaults(); refresh(); }
    void save() override { KCModule::save(); m_config->sync(); }

private:
    void refresh()
    {
        if (!m_form) return;
        const int factor = m_subdivide->isChecked() ? 2 : 1;
        const int columns = m_columns->value() * factor, rows = m_rows->value() * factor;
        m_preview->setColumnCount(columns);
        m_preview->setRowCount(rows);
        m_form->findChild<QLabel *>("previewCaption")->setText(
            QStringLiteral("%1 × %2 · %3 zones").arg(columns).arg(rows).arg(columns * rows));
        const bool custom = m_custom->isChecked();
        for (auto *spin : {m_columns, m_rows, m_gap}) spin->setEnabled(!custom);
        m_subdivide->setEnabled(!custom);
        m_preview->setEnabled(!custom);
        m_form->findChild<QLabel *>("layoutModeHint")->setVisible(custom);
        m_json->setEnabled(custom);
        QString error;
        if (custom) {
            if (m_load.isCallable()) {
                const QJSValue result = m_load.call({m_json->toPlainText(), columns, rows, m_gap->value(), false, true});
                error = result.isError() ? result.toString() : result.property(QStringLiteral("error")).toString();
            } else {
                error = QStringLiteral("Could not load the custom layout validator. Using the default 4×2 grid.");
            }
        }
        m_error->setText(error);
        m_error->setVisible(!error.isEmpty());
    }

    KSharedConfig::Ptr m_config;
    QJSEngine m_engine;
    QJSValue m_load;
    QWidget *m_form = nullptr;
    QSpinBox *m_columns = nullptr, *m_rows = nullptr, *m_gap = nullptr;
    QCheckBox *m_subdivide = nullptr, *m_custom = nullptr;
    QPlainTextEdit *m_json = nullptr;
    QLabel *m_error = nullptr;
    QTableWidget *m_preview = nullptr;
};

K_PLUGIN_CLASS_WITH_JSON(KFancyZonesConfig, "settingsplugin.json")
#include "settingsplugin.moc"
