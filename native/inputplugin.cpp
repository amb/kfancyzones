// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QQmlExtensionPlugin>
#include <qqml.h>

#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>

// Runs inside KWin. Observe modifier state only; never grab or inject input.
class InputState : public QObject, public KWin::InputEventSpy
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(int modifiers READ modifiers NOTIFY modifiersChanged)

public:
    explicit InputState(QObject *parent = nullptr)
        : QObject(parent)
    {
        if (auto *input = KWin::input()) {
            connect(input, &KWin::InputRedirection::keyboardModifiersChanged,
                    this, &InputState::modifiersChanged);
            input->installInputEventSpy(this);
        }
    }

    bool available() const { return KWin::input() != nullptr; }
    int modifiers() const
    {
        auto *input = KWin::input();
        return input ? int(input->keyboardModifiers()) : 0;
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
