#include "DSDetachedWindow.h"

#include <QCursor>
#include <QEvent>
#include <QMoveEvent>
#include <QPoint>

CDSDetachedWindow::CDSDetachedWindow(QWidget *dockTarget, QWidget *content,
                                     const QString &title, const QIcon &icon,
                                     QWidget *parent)
    : QMainWindow(parent)
    , m_dockTarget(dockTarget)
{
    setWindowTitle(title);
    setWindowIcon(icon);
    // 关闭 = 关掉这个网页：窗口销毁时中央部件（网页窗口）一并销毁
    setAttribute(Qt::WA_DeleteOnClose);

    // 接手被拖出的网页窗口。注意：这里用 setCentralWidget 接管所有权。
    setCentralWidget(content);

    // 尺寸取停靠目标的三分之二（不小于 720×520），让独立窗口看起来仍是“同一块内容”
    int w = 920;
    int h = 640;
    if (m_dockTarget) {
        w = qMax(720, m_dockTarget->width() * 2 / 3);
        h = qMax(520, m_dockTarget->height() * 2 / 3);
    }
    resize(w, h);
}

CDSDetachedWindow::~CDSDetachedWindow() = default;

bool CDSDetachedWindow::overDockTarget() const
{
    if (!m_dockTarget || !m_dockTarget->isVisible()) {
        return false;
    }
    // 以光标位置判定：拖动过程中光标是用户真正的指向
    return m_dockTarget->rect().contains(m_dockTarget->mapFromGlobal(QCursor::pos()));
}

void CDSDetachedWindow::updateDockState()
{
    const bool over = overDockTarget();
    if (!over) {
        // 记下“确实被拖出去过”：只有离开过目标，后面的停靠才算“拖回”（见头文件说明）
        m_everLeftTarget = true;
    }
    if (over == m_overTarget) {
        return;
    }
    m_overTarget = over;
    emit dockHintChanged(over);
}

void CDSDetachedWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    updateDockState();
}

bool CDSDetachedWindow::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonRelease:
    case QEvent::NonClientAreaMouseButtonRelease:
        // 松开鼠标：按“松开那一刻”的最终位置判定，避免 moveEvent 的滞后
        updateDockState();
        if (m_overTarget && m_everLeftTarget) {
            m_overTarget = false;
            emit dockHintChanged(false); // 先收起高亮，再请求停靠
            emit dockRequested();
        }
        break;
    default:
        break;
    }
    return QMainWindow::event(event);
}
