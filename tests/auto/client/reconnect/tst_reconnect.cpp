// Copyright (C) 2023 David Edmundson <davidedmundson@kde.org>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mockcompositor.h"

#include <QBackingStore>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QMimeData>
#include <QPixmap>
#include <QDrag>
#include <QWindow>
#if QT_CONFIG(opengl)
#include <QOpenGLWindow>
#endif
#include <QRasterWindow>

#include <QtTest/QtTest>
#include <QtWaylandClient/private/qwaylandintegration_p.h>
#include <QtGui/private/qguiapplication_p.h>

#include "wl-socket.h"

using namespace MockCompositor;

class TestWindow : public QRasterWindow
{
public:
    TestWindow()
    {
    }

    void focusInEvent(QFocusEvent *) override
    {
        ++focusInEventCount;
    }

    void focusOutEvent(QFocusEvent *) override
    {
        ++focusOutEventCount;
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        ++keyPressEventCount;
        keyCode = event->nativeScanCode();
    }

    void keyReleaseEvent(QKeyEvent *event) override
    {
        ++keyReleaseEventCount;
        keyCode = event->nativeScanCode();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        ++mousePressEventCount;
        mousePressPos = event->position().toPoint();
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        ++mouseReleaseEventCount;
    }

    void touchEvent(QTouchEvent *event) override
    {
        Q_UNUSED(event);
        ++touchEventCount;
    }

    QPoint frameOffset() const { return QPoint(frameMargins().left(), frameMargins().top()); }

    int focusInEventCount = 0;
    int focusOutEventCount = 0;
    int keyPressEventCount = 0;
    int keyReleaseEventCount = 0;
    int mousePressEventCount = 0;
    int mouseReleaseEventCount = 0;
    int touchEventCount = 0;

    uint keyCode = 0;
    QPoint mousePressPos;
};

class tst_WaylandReconnect : public QObject
{
    Q_OBJECT
public:
    tst_WaylandReconnect();
    void triggerReconnect();

    template<typename function_type, typename... arg_types>
    auto exec(function_type func, arg_types&&... args) -> decltype(func())
    {
        return m_comp->exec(func, std::forward<arg_types>(args)...);
    }

private Q_SLOTS:
//core
    void cleanup() { QTRY_VERIFY2(m_comp->isClean(), qPrintable(m_comp->dirtyMessage())); }
    void basicWindow();

//input
    void keyFocus();

private:
    void configureWindow();
    QScopedPointer<DefaultCompositor> m_comp;
    wl_socket *m_socket;
};

tst_WaylandReconnect::tst_WaylandReconnect()
{
    m_socket = wl_socket_create();
    QVERIFY(m_socket);
    const int socketFd = wl_socket_get_fd(m_socket);
    const QByteArray socketName = wl_socket_get_display_name(m_socket);
    qputenv("WAYLAND_DISPLAY", socketName);

    m_comp.reset(new DefaultCompositor(CoreCompositor::Default, dup(socketFd)));
}

void tst_WaylandReconnect::triggerReconnect()
{
    const int socketFd = wl_socket_get_fd(m_socket);
    m_comp.reset(new DefaultCompositor(CoreCompositor::Default, dup(socketFd)));
    QTest::qWait(50); //we need to spin the main loop to actually reconnect
}

void tst_WaylandReconnect::basicWindow()
{
    QRasterWindow window;
    window.resize(64, 48);
    window.show();
    QCOMPOSITOR_TRY_VERIFY(m_comp->xdgToplevel());

    triggerReconnect();

    QCOMPOSITOR_TRY_VERIFY(m_comp->xdgToplevel());
}

void tst_WaylandReconnect::keyFocus()
{
    TestWindow window;
    window.resize(64, 48);
    window.show();

    configureWindow();
    QTRY_VERIFY(window.isExposed());
    exec([=] {
        m_comp->keyboard()->sendEnter(m_comp->surface());
    });
    QTRY_COMPARE(window.focusInEventCount, 1);

    uint keyCode = 80;
    QCOMPARE(window.keyPressEventCount, 0);
    exec([=] {
        m_comp->keyboard()->sendKey(m_comp->client(), keyCode - 8, Keyboard::key_state_pressed);
    });
    QTRY_COMPARE(window.keyPressEventCount, 1);
    QCOMPARE(QGuiApplication::focusWindow(), &window);

    triggerReconnect();
    configureWindow();

    // on reconnect our knowledge of focus is reset to a clean slate
    QCOMPARE(QGuiApplication::focusWindow(), nullptr);
    QTRY_COMPARE(window.focusOutEventCount, 1);

    // fake the user explicitly focussing this window afterwards
    exec([=] {
        m_comp->keyboard()->sendEnter(m_comp->surface());
    });
    exec([=] {
        m_comp->keyboard()->sendKey(m_comp->client(), keyCode - 8, Keyboard::key_state_pressed);
    });
    QTRY_COMPARE(window.focusInEventCount, 2);
    QTRY_COMPARE(window.keyPressEventCount, 2);
}


void tst_WaylandReconnect::configureWindow()
{
    QCOMPOSITOR_TRY_VERIFY(m_comp->xdgToplevel());
    m_comp->exec([=] {
        m_comp->xdgToplevel()->sendConfigure({0, 0}, {});
        const uint serial = m_comp->nextSerial(); // Let the window decide the size
        m_comp->xdgSurface()->sendConfigure(serial);
    });
}

int main(int argc, char **argv)
{
    // Note when debugging that a failing reconnect will exit this
    // test rather than fail. Making sure it finishes is important!

    QTemporaryDir tmpRuntimeDir;
    setenv("QT_QPA_PLATFORM", "wayland", 1); // force QGuiApplication to use wayland plugin
    setenv("QT_WAYLAND_RECONNECT", "1", 1);
    setenv("XDG_CURRENT_DESKTOP", "qtwaylandtests", 1);

    tst_WaylandReconnect tc;
    QGuiApplication app(argc, argv);
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_reconnect.moc"
