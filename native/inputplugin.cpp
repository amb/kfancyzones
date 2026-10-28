// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QPointer>
#include <QQmlExtensionPlugin>
#include <qqml.h>

#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>
#include <window.h>
#include <workspace.h>

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
    Q_PROPERTY(QObject *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool suppressNativeTiling READ suppressNativeTiling WRITE setSuppressNativeTiling NOTIFY suppressNativeTilingChanged)

public:
    explicit InputState(QObject *parent = nullptr)
        : QObject(parent)
    {
        if (auto *input = KWin::input()) {
            connect(input, &KWin::InputRedirection::keyboardModifiersChanged,
                    this, &InputState::modifiersChanged);
            input->installInputEventSpy(this);
            input->installInputEventFilter(&m_moveFilter);
        }
    }

    bool available() const { return KWin::input() != nullptr; }
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

private:
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

#include "inputplugin.moc"
