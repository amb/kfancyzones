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
#include <QTabWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace KFancyZonesSettingsV12
{

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
        createForm();
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
    void createForm()
    {
        m_form = new QWidget(widget());
        m_form->setObjectName(QStringLiteral("KFancyZonesSettings"));
        m_form->setMinimumWidth(480);
        auto *container = new QVBoxLayout(widget());
        container->addWidget(m_form);
        auto *shell = new QVBoxLayout(m_form);
        auto *tabs = new QTabWidget(m_form);
        tabs->setObjectName(QStringLiteral("settingsTabs"));
        shell->addWidget(tabs);

        const auto label = [](QWidget *parent, const char *name, const QString &text) {
            auto *item = new QLabel(text, parent);
            item->setObjectName(QString::fromLatin1(name));
            item->setWordWrap(true);
            return item;
        };
        auto *main = new QWidget(tabs);
        main->setObjectName(QStringLiteral("layoutTab"));
        tabs->addTab(main, tr("Layout"));
        auto *layout = new QVBoxLayout(main);
        layout->setSpacing(12);
        layout->addWidget(label(main, "layoutHeading", tr("<b>Layout</b>")));
        auto *controls = new QHBoxLayout;
        layout->addLayout(controls);
        const auto spin = [&](const QString &title, const char *name, int minimum, int maximum, int value) {
            auto *caption = new QLabel(title, main);
            auto *item = new QSpinBox(main);
            item->setObjectName(QString::fromLatin1(name));
            item->setRange(minimum, maximum);
            item->setValue(value);
            caption->setBuddy(item);
            controls->addWidget(caption);
            controls->addWidget(item);
            return item;
        };
        m_columns = spin(tr("Columns"), "kcfg_gridColumns", 1, 8, 4);
        m_rows = spin(tr("Rows"), "kcfg_gridRows", 1, 4, 2);
        m_gap = spin(tr("Gap"), "kcfg_gridGap", 0, 100, 0);
        m_gap->setSuffix(tr(" px"));
        m_subdivide = new QCheckBox(tr("Subdivide each zone into 2×2"), main);
        m_subdivide->setObjectName(QStringLiteral("kcfg_subdivideZones"));
        m_subdivide->setToolTip(tr("Doubles the columns and rows. The default 4×2 grid becomes 8×4."));
        layout->addWidget(m_subdivide);
        auto *hint = label(main, "layoutModeHint",
            tr("Custom layout is enabled on the Advanced tab. Turn it off to use these grid settings."));
        hint->hide();
        layout->addWidget(hint);
        auto *caption = label(main, "previewCaption", QString());
        caption->setAlignment(Qt::AlignCenter);
        layout->addWidget(caption);
        m_preview = new QTableWidget(2, 4, main);
        m_preview->setObjectName(QStringLiteral("layoutPreview"));
        m_preview->setFixedSize(400, 144);
        m_preview->setFocusPolicy(Qt::NoFocus);
        m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_preview->setSelectionMode(QAbstractItemView::NoSelection);
        m_preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        layout->addWidget(m_preview, 0, Qt::AlignHCenter);
        layout->addWidget(label(main, "gestureHelp", tr(
            "<b>Shift + drag</b> selects a zone. Add <b>Ctrl</b> to combine zones.<br/>"
            "Release the <b>mouse first</b> to accept, or release <b>Shift first</b> to cancel.")));
        auto *shortcuts = label(main, "shortcutsLink",
            tr("<a href=\"systemsettings://kcm_keys\">Configure keyboard shortcuts…</a>"));
        shortcuts->setOpenExternalLinks(true);
        layout->addWidget(shortcuts);
        layout->addWidget(label(main, "shortcutsHint", tr("Search for KFancyZones in the Shortcuts page.")));
        layout->addStretch();

        auto *advanced = new QWidget(tabs);
        advanced->setObjectName(QStringLiteral("advancedOptions"));
        tabs->addTab(advanced, tr("Advanced"));
        auto *advancedLayout = new QVBoxLayout(advanced);
        m_custom = new QCheckBox(tr("Use custom layout"), advanced);
        m_custom->setObjectName(QStringLiteral("kcfg_useCustomLayout"));
        advancedLayout->addWidget(m_custom);
        advancedLayout->addWidget(label(advanced, "customLayoutsHelp", tr(
            "Enable to use custom JSON instead of the grid controls. Invalid JSON uses the default 4×2 grid.")));
        m_json = new QPlainTextEdit(advanced);
        m_json->setObjectName(QStringLiteral("kcfg_layoutsJson"));
        m_json->setPlaceholderText(tr("Layout JSON"));
        m_json->setMinimumHeight(140);
        advancedLayout->addWidget(m_json);
        m_error = label(advanced, "customLayoutError", QString());
        m_error->setTextFormat(Qt::PlainText);
        m_error->setStyleSheet(QStringLiteral("color: #da4453;"));
        m_error->hide();
        advancedLayout->addWidget(m_error);
        advancedLayout->addWidget(label(advanced, "applicationsLabel", tr("<b>Applications</b>")));
        auto *filter = new QComboBox(advanced);
        filter->setObjectName(QStringLiteral("kcfg_filterMode"));
        filter->addItems({tr("Only include these applications"), tr("Exclude these applications")});
        filter->setCurrentIndex(1);
        advancedLayout->addWidget(filter);
        auto *applications = new QPlainTextEdit(advanced);
        applications->setObjectName(QStringLiteral("kcfg_filterList"));
        applications->setPlaceholderText(tr("One window class per line (for example, org.kde.konsole)"));
        applications->setMaximumHeight(72);
        advancedLayout->addWidget(applications);
        advancedLayout->addStretch();
        shell->addWidget(label(m_form, "applyHelp", tr("Changes take effect when you click Apply.")));
    }

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
} // namespace KFancyZonesSettingsV12

#include "settingsplugin.moc"
