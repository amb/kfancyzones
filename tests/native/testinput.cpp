// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only driver. Never packaged or installed with the script.
#include <QObject>
#include <QQmlExtensionPlugin>
#include <qqml.h>
#include <input.h>
#include <keyboard_input.h>
#include <pointer_input.h>
#include <outline.h>
#include <workspace.h>

class TestInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int outlineShows READ outlineShows NOTIFY outlineShowsChanged)
public:
    explicit TestInput(QObject *parent = nullptr) : QObject(parent)
    {
        auto *outline = KWin::workspace()->outline();
        connect(outline, &KWin::Outline::activeChanged, this, [this, outline] {
            if (outline->isActive()) {
                ++m_outlineShows;
                Q_EMIT outlineShowsChanged();
            }
        });
    }

    int outlineShows() const { return m_outlineShows; }

    Q_INVOKABLE void key(int code, bool down)
    {
        if (!enabled()) return;
        KWin::input()->keyboard()->processKey(code,
            down ? KWin::KeyboardKeyState::Pressed : KWin::KeyboardKeyState::Released, time());
    }

    Q_INVOKABLE void pointer(int x, int y)
    {
        if (enabled()) KWin::input()->pointer()->processMotionAbsolute(QPointF(x,y), time());
    }

    Q_INVOKABLE void button(bool down)
    {
        if (enabled()) KWin::input()->pointer()->processButton(0x110,
            down ? KWin::PointerButtonState::Pressed : KWin::PointerButtonState::Released, time());
    }

Q_SIGNALS:
    void outlineShowsChanged();

private:
    int m_outlineShows = 0;
    bool enabled() const
    {
        return qEnvironmentVariable("KFANCYZONES_INTEGRATION_TEST") == QLatin1String("1") && KWin::input();
    }
    std::chrono::microseconds time() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    }
};

class TestInputPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override { qmlRegisterType<TestInput>(uri,1,0,"TestInput"); }
};

#include "testinput.moc"
