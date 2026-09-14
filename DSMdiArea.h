#ifndef DSMDIAREA_H
#define DSMDIAREA_H

#include <QIcon>
#include <QMdiArea>
#include <QPoint>
#include <QString>

class QFrame;
class QMdiSubWindow;
class QResizeEvent;
class QShowEvent;
class QTabBar;

// PocketWeb 使用的 MDI 区域（TabbedView），放在「网页」页签里。
//
// 【为什么需要这个子类 —— 空标签栏】
// QMdiArea 在 TabbedView 下会自建一个内部标签栏（qmdiarea.cpp:1536-1538
// new QMdiAreaTabBar(q)），并且显示它是**无条件**的 —— 与标签数量无关
// （qmdiarea.cpp:1573-1574）；同时它会按该标签栏的高度预留视口上边距
// （qmdiarea.cpp:1614,1628-1629）。于是「一个子窗口都没有」时：
//   - 空标签栏会在 MDI 灰底上留下一条白色基线
//     （qtabbar.cpp:1860-1866：没有标签时循环不执行，只剩
//      drawPrimitive(PE_FrameTabBarBase)；qtabbar_p.h:228-248 把这条基线
//      画在标签栏底部、宽度取标签栏宽度）；
//   - 还多出一行被预留的高度。
// 本类在「没有子窗口」时把内部标签栏隐藏并把视口上边距清零；有子窗口时什么都不做，
// 完全交回 QMdiArea 管理（appendChild() 自己会调 updateTabBarGeometry()，
// qmdiarea.cpp:825-827）。
//
// 【标签拖出 / 拖回】
// 本类只负责 MDI 区域这一侧的三件事：
//   1) 在内部标签栏上识别“按住标签往外拖”（超过 QApplication::startDragDistance()），
//      发出 tabDragOutRequested()，由宿主把它变成独立窗口（见 CDSDetachedWindow）；
//   2) 拖回过程中显示/隐藏停靠高亮边框（setDockHintVisible()）；
//   3) 重新停靠：dockBack() 把网页重新加成 MDI 子窗口，激活对应标签并最大化。
//
// 【刻意不切换 viewMode】
// Qt 在切到 TabbedView 时会对已最大化的当前子窗口调用 showNormal()
// （qmdiarea.cpp:1560-1561），而本工程的子窗口都是 showMaximized() 打开的
// （网页视图最怕这种 隐藏/显示/还原 反复折腾），所以这里不碰 viewMode。
class CDSMdiArea : public QMdiArea
{
    Q_OBJECT

public:
    explicit CDSMdiArea(QWidget *parent = nullptr);
    ~CDSMdiArea() override;

    // 把 content 作为新的 MDI 子窗口放回本区域：设置标题/图标、最大化并激活对应标签。
    // 返回新建的子窗口（失败返回 nullptr）。用于独立窗口被拖回时。
    QMdiSubWindow *dockBack(QWidget *content, const QString &title, const QIcon &icon);

    // 停靠高亮提示：拖动独立窗口经过本区域时显示一个蓝色边框（不拦截鼠标）
    void setDockHintVisible(bool visible);

signals:
    // 用户把某个标签拖出了标签栏（超过起始拖动距离）→ 宿主应把它变成独立窗口。
    // 注意：本信号是在标签栏**自己的鼠标事件**里发出的，宿主必须排队处理，
    // 不能同步移除子窗口（那会在标签栏处理鼠标事件的过程中改动标签栏，造成重入）。
    void tabDragOutRequested(QMdiSubWindow *subWindow);

protected:
    // 尺寸变化后 QMdiArea 会重算视口边距，这里再校正一次空状态并同步高亮边框
    void resizeEvent(QResizeEvent *event) override;
    // 首次显示时内部标签栏才真正可见，这里校正一次空状态
    void showEvent(QShowEvent *event) override;
    // 监听 viewport 的子对象增删（感知有没有子窗口）与内部标签栏的鼠标事件（识别拖出）
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // 空状态同步 + 保证“拖出”事件过滤器挂在当前的内部标签栏上。
    // 无子窗口 → 隐藏内部标签栏 + 视口上边距清零；有子窗口 → 标签栏正常显示。
    void syncEmptyState();
    // QMdiArea 的内部标签栏；非 TabbedView 时返回 nullptr
    QTabBar *internalTabBar() const;
    // 按标签序号取子窗口（标签序号与 subWindowList() 同序，见 qmdiarea.cpp:1097）
    QMdiSubWindow *subWindowAt(int index) const;
    // 懒创建停靠高亮边框
    void ensureDockHint();

    QTabBar *m_watchedTabBar = nullptr; // 已挂事件过滤器的内部标签栏（非 TabbedView 时为 nullptr）
    QFrame *m_dockHint = nullptr;       // 停靠高亮边框（懒创建）
    int m_dragIndex = -1;               // 按下时命中的标签序号（-1 = 没有）
    QPoint m_dragStartPos;              // 按下时的全局坐标
    bool m_dragOutEmitted = false;      // 本次拖动是否已发出过 tabDragOutRequested
};

#endif // DSMDIAREA_H
