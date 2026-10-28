// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QPointer>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>
#include <QDir>
#include <QFileInfo>
#include <KConfig>
#include <KConfigGroup>
#include <QQmlExtensionPlugin>
#include <qqml.h>

#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>
#include <window.h>
#include <workspace.h>

namespace KFancyZonesLiveInput
{

// Run before KWin's move filter (equal weights are inserted before existing
// filters), after effects and global shortcuts. Only the owned Shift-move is
// handled here; button release, resizing and other windows use KWin as usual.
class ZoneMoveFilter : public KWin::InputEventFilter
{
public:
    ZoneMoveFilter() : InputEventFilter(KWin::InputFilterOrder::InteractiveMoveResize) {}

    QPointer<KWin::Window> window;
    bool enabled = false;

    bool ownsMove(Qt::KeyboardModifiers modifiers) const
    {
        return enabled && window && (modifiers & Qt::ShiftModifier)
            && window->isInteractiveMove()
            && KWin::workspace()->moveResizeWindow() == window;
    }

    bool pointerMotion(KWin::PointerMotionEvent *event) override
    {
        if (!ownsMove(event->modifiers)) return false;
        // Keep real modifier state intact for QML. Strip Shift only from the
        // move update so KWin never creates its competing custom-tile outline.
        window->updateInteractiveMoveResize(event->position, event->modifiers & ~Qt::ShiftModifier);
        return true;
    }

    bool keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        if (!ownsMove(KWin::input()->keyboardModifiers())) return false;
        if (event->state == KWin::KeyboardKeyState::Pressed
            || event->state == KWin::KeyboardKeyState::Repeated) {
            QPointer<KWin::Window> target = window;
            target->keyPressEvent(QKeyCombination{event->modifiers, event->key});
            if (target && target->isInteractiveMove()) {
                target->updateInteractiveMoveResize(KWin::input()->globalPointer(),
                    KWin::input()->keyboardModifiers() & ~Qt::ShiftModifier);
            }
        }
        return true;
    }
};

// Runs inside KWin; observes raw modifiers and owns one scoped move filter.
class InputState : public QObject, public KWin::InputEventSpy
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(int modifiers READ modifiers NOTIFY modifiersChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration NOTIFY configurationChanged)
    Q_PROPERTY(QObject *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool suppressNativeTiling READ suppressNativeTiling WRITE setSuppressNativeTiling NOTIFY suppressNativeTilingChanged)

public:
    explicit InputState(QObject *parent = nullptr)
        : QObject(parent)
        , m_config(QStringLiteral("kwinrc"), KConfig::SimpleConfig)
    {
        m_reloadTimer.setSingleShot(true);
        m_reloadTimer.setInterval(75);
        connect(&m_reloadTimer, &QTimer::timeout, this, &InputState::reloadConfiguration);
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_reloadTimer.start(); });
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reloadTimer.start(); });
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QDir().mkpath(directory);
        m_watcher.addPath(directory);
        m_configPath = directory + QStringLiteral("/kwinrc");
        reloadConfiguration();
        if (auto *input = KWin::input()) {
            connect(input, &KWin::InputRedirection::keyboardModifiersChanged,
                    this, &InputState::modifiersChanged);
            input->installInputEventSpy(this);
            input->installInputEventFilter(&m_moveFilter);
        }
    }

    bool available() const { return KWin::input() != nullptr; }
    QVariantMap configuration() const { return m_configuration; }
    int modifiers() const
    {
        auto *input = KWin::input();
        return input ? int(input->keyboardModifiers()) : 0;
    }

    QObject *window() const { return m_moveFilter.window; }
    void setWindow(QObject *window)
    {
        auto *target = qobject_cast<KWin::Window *>(window);
        if (m_moveFilter.window == target) return;
        m_moveFilter.window = target;
        Q_EMIT windowChanged();
    }
    bool suppressNativeTiling() const { return m_moveFilter.enabled; }
    void setSuppressNativeTiling(bool enabled)
    {
        if (m_moveFilter.enabled == enabled) return;
        m_moveFilter.enabled = enabled;
        Q_EMIT suppressNativeTilingChanged();
    }

    void keyboardKey(KWin::KeyboardKeyEvent *event) override
    {
        // Observe Escape before KWin's move filter emits move-finished.
        if (event->key == Qt::Key_Escape && event->state == KWin::KeyboardKeyState::Pressed) {
            Q_EMIT cancelRequested();
        }
    }

Q_SIGNALS:
    void modifiersChanged();
    void cancelRequested();
    void windowChanged();
    void suppressNativeTilingChanged();
    void configurationChanged();

private:
    void reloadConfiguration()
    {
        // KConfig can replace the file atomically; restore the watch afterward.
        if (QFileInfo::exists(m_configPath) && !m_watcher.files().contains(m_configPath)) {
            m_watcher.addPath(m_configPath);
        }
        m_config.reparseConfiguration();
        const KConfigGroup group(&m_config, QStringLiteral("Script-kfancyzones"));
        const QVariantMap updated{
            {QStringLiteral("gridColumns"), group.readEntry("gridColumns", 4)},
            {QStringLiteral("gridRows"), group.readEntry("gridRows", 2)},
            {QStringLiteral("gridGap"), group.readEntry("gridGap", 0)},
            {QStringLiteral("subdivideZones"), group.readEntry("subdivideZones", false)},
            {QStringLiteral("useCustomLayout"), group.readEntry("useCustomLayout", false)},
            {QStringLiteral("layoutsJson"), group.readEntry("layoutsJson", QString())},
            {QStringLiteral("filterMode"), group.readEntry("filterMode", 1)},
            {QStringLiteral("filterList"), group.readEntry("filterList", QString())},
            {QStringLiteral("enableDebugLogging"), group.readEntry("enableDebugLogging", false)},
        };
        if (updated != m_configuration) {
            m_configuration = updated;
            Q_EMIT configurationChanged();
        }
    }

    KConfig m_config;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QString m_configPath;
    QVariantMap m_configuration;
    ZoneMoveFilter m_moveFilter;
};

class InputPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<InputState>(uri, 1, 0, "InputState");
    }
};

} // namespace KFancyZonesLiveInput

#include "inputplugin.moc"
