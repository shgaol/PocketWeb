#include "DSMdiArea.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTabBar>

CDSMdiArea::CDSMdiArea(QWidget *parent)
    : QMdiArea(parent)
{
    // 子窗口是 viewport 的子对象（qmdiarea.cpp:1965 / 1968 以 viewport 为父），
    // 因此监听 viewport 的子对象增删就能感知「有没有子窗口」。
    viewport()->installEventFilter(this);

    // 构造时（还没设 TabbedView）标签栏还不存在，这次调用是无害的空操作；
    // 设成 TabbedView 之后，showEvent 会再做一次。
    syncEmptyState();
}

CDSMdiArea::~CDSMdiArea() = default;

QTabBar *CDSMdiArea::internalTabBar() const
{
    // QMdiArea 在 TabbedView 下创建的内部标签栏是本对象的直接子对象，类型为 QTabBar。
    // 非 TabbedView 时它不存在，这里返回 nullptr，调用方直接跳过（取不到就什么都不做，
    // 不会影响正常显示）。
    return findChild<QTabBar *>();
}

QMdiSubWindow *CDSMdiArea::subWindowAt(int index) const
{
    const QList<QMdiSubWindow *> subWindows = subWindowList();
    if (index < 0 || index >= subWindows.size()) {
        return nullptr;
    }
    return subWindows.at(index);
}

void CDSMdiArea::syncEmptyState()
{
    QTabBar *bar = internalTabBar();

    // 内部标签栏是在 setViewMode(TabbedView) 之后才存在的（qmdiarea.cpp:1538），
    // 所以构造时它还不存在，不能在那里挂事件过滤器。这里每次同步都直接挂一遍 ——
    // installEventFilter() 对同一个过滤器是幂等的（已存在则只是移到末尾，不会重复挂），
    // 因此即使标签栏将来被重建、甚至恰好复用了同一地址，也不会漏挂。
    // 本函数在 show / resize / 子窗口增删后都会被调用；而“要有标签可拖，必然先加过
    // 子窗口”，加子窗口会触发 viewport 的 ChildAdded → 排队调用本函数，
    // 所以用户能拖之前过滤器一定已经挂好。
    if (bar) {
        bar->installEventFilter(this);
        m_watchedTabBar = bar;
    }

    if (!bar) {
        return; // 非 TabbedView：没有内部标签栏，也没有空状态要处理
    }

    if (subWindowList().isEmpty()) {
        // 空状态：藏掉标签栏（连带它画的那条基线），并把为它预留的一行视口边距清零
        bar->hide();
        setViewportMargins(0, 0, 0, 0);
    } else {
        // 有子窗口：恢复显示即可，边距与几何由 QMdiArea 自己算好，这里不插手
        bar->show();
    }
}

void CDSMdiArea::ensureDockHint()
{
    if (m_dockHint) {
        return;
    }
    m_dockHint = new QFrame(this);
    m_dockHint->setObjectName(QStringLiteral("dockHint"));
    // 不拦鼠标：拖动过程中光标要能“穿过”它继续参与判定
    m_dockHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    // 品牌蓝边框 + 淡蓝填充，在灰色 MDI 底上对比明显（不依赖系统调色板）
    m_dockHint->setStyleSheet(QStringLiteral(
        "QFrame#dockHint {"
        "  border: 3px solid #2563EB;"
        "  background-color: rgba(37, 99, 235, 48);"
        "}"));
    m_dockHint->hide();
}

void CDSMdiArea::setDockHintVisible(bool visible)
{
    if (!visible) {
        if (m_dockHint) {
            m_dockHint->hide();
        }
        return;
    }
    ensureDockHint();
    m_dockHint->setGeometry(rect());
    m_dockHint->raise();
    m_dockHint->show();
}

QMdiSubWindow *CDSMdiArea::dockBack(QWidget *content, const QString &title, const QIcon &icon)
{
    if (!content) {
        return nullptr;
    }
    setDockHintVisible(false);

    QMdiSubWindow *sub = addSubWindow(content);
    sub->setWindowTitle(title);
    sub->setWindowIcon(icon);
    sub->setAttribute(Qt::WA_DeleteOnClose);
    // 需求：拖回的窗口回到页签上并最大化；同时激活它（自动激活网页标签）
    sub->showMaximized();
    setActiveSubWindow(sub);
    return sub;
}

void CDSMdiArea::resizeEvent(QResizeEvent *event)
{
    QMdiArea::resizeEvent(event); // 内部会重算标签栏几何与视口边距
    syncEmptyState();
    if (m_dockHint && m_dockHint->isVisible()) {
        m_dockHint->setGeometry(rect());
    }
}

void CDSMdiArea::showEvent(QShowEvent *event)
{
    QMdiArea::showEvent(event);
    syncEmptyState();
}

bool CDSMdiArea::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == viewport()) {
        switch (event->type()) {
        case QEvent::ChildAdded:
        case QEvent::ChildRemoved:
            // ChildAdded 发生在 QMdiAreaPrivate::appendChild() 之前（子窗口在构造时
            // 就以 viewport 为父，见 qmdiarea.cpp:1968），此刻 childWindows 尚未更新，
            // 所以延到事件循环下一轮再按最终状态同步。
            // 这里用排队的元对象调用代替定时器（语义等价，且不引入 QTimer）。
            QMetaObject::invokeMethod(this, [this]() { syncEmptyState(); },
                                      Qt::QueuedConnection);
            break;
        default:
            break;
        }
    }

    if (watched == m_watchedTabBar) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragIndex = m_watchedTabBar->tabAt(me->position().toPoint());
                m_dragStartPos = me->globalPosition().toPoint();
                m_dragOutEmitted = false;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_dragIndex >= 0 && !m_dragOutEmitted
                && (me->buttons() & Qt::LeftButton)
                && (me->globalPosition().toPoint() - m_dragStartPos).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_dragOutEmitted = true; // 一次拖动只触发一次
                if (QMdiSubWindow *sub = subWindowAt(m_dragIndex)) {
                    m_dragIndex = -1;
                    emit tabDragOutRequested(sub);
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            m_dragIndex = -1;
            m_dragOutEmitted = false;
            break;
        default:
            break;
        }
    }

    return QMdiArea::eventFilter(watched, event);
}
