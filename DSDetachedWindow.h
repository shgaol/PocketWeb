#ifndef DSDETACHEDWINDOW_H
#define DSDETACHEDWINDOW_H

#include <QIcon>
#include <QMainWindow>
#include <QString>

class QMoveEvent;

// 从「网页」页签的标签栏拖出来的网页，装在这个独立顶级窗口里。
//
// 【怎么知道用户“松手了”】
// Windows 在标题栏上按下时只发 WM_NCLBUTTONDOWN，拖动结束后**不发** WM_NCLBUTTONUP，
// 只发 WM_NCMOUSEMOVE/WM_MOUSEMOVE；Qt 在 WM_EXITSIZEMOVE 时主动合成这个松开事件
// （qtbase/src/plugins/platforms/windows/qwindowscontext.cpp:1261-1290 的
//  handleExitSizeMove()），并且按“光标是否落在窗口矩形内”选择事件类型：
//   窗口内 → QEvent::MouseButtonRelease
//   窗口外 → QEvent::NonClientAreaMouseButtonRelease
// 拖回网页页签时鼠标正是在窗口外，所以两种类型都要接。
// 该事件经 QWidgetWindow::handleNonClientAreaMouseEvent() →
// QApplication::forwardEvent() 直接投递给本窗口（qwidgetwindow.cpp:502-505）。
//
// 【判定规则】
// 以「松开那一刻光标是否落在停靠目标（网页页签里的 MDI 区域）矩形内」为准 ——
// 与用户“把窗口拖回去、对着区域松手”的直觉一致，也不受窗口大小影响。
// 拖动过程中一旦进入/离开目标就发 dockHintChanged()，由 MDI 区域显示高亮边框。
//
// 另外还要求「至少离开过目标一次」（m_everLeftTarget）：新窗口就生成在光标处，
// 那一刻光标本来就在目标区域里；若不加这个条件，用户刚把标签拖出来、手一松就会
// 立刻被停靠回去 —— 看起来就像“根本拖不出来”。加上之后语义恰好是
// 「先拖出去、再拖回来」才停靠，与需求描述完全一致。
//
// 关闭本窗口 = 关掉这个网页（与在 MDI 里点标签的 × 语义一致）：窗口带
// WA_DeleteOnClose，中央部件（网页窗口）随窗口一起销毁。
class CDSDetachedWindow : public QMainWindow
{
    Q_OBJECT

public:
    // dockTarget 停靠目标（「网页」页签里的 MDI 区域）；content 被拖出的网页窗口。
    // 内容的所有权在本窗口（show 之后由中央部件持有）。
    CDSDetachedWindow(QWidget *dockTarget, QWidget *content,
                      const QString &title, const QIcon &icon,
                      QWidget *parent = nullptr);
    ~CDSDetachedWindow() override;

signals:
    // “是否落在停靠目标上”发生变化（true = 可以放下）→ 显示/隐藏高亮提示
    void dockHintChanged(bool visible);
    // 松开鼠标时仍落在停靠目标上 → 请求重新停靠回「网页」页签
    void dockRequested();

protected:
    // 拖动过程中实时更新“是否落在停靠目标上”
    void moveEvent(QMoveEvent *event) override;
    // 接收 Qt 合成的松开事件（见类注释）
    bool event(QEvent *event) override;

private:
    // 光标当前是否落在停靠目标矩形内
    bool overDockTarget() const;
    // 重新计算并（必要时）发出 dockHintChanged()
    void updateDockState();

    QWidget *m_dockTarget = nullptr; // 停靠目标（MDI 区域），不持有
    bool m_overTarget = false;       // 当前是否算“落在停靠目标上”
    bool m_everLeftTarget = false;   // 生成之后是否至少离开过目标一次（见类注释）
};

#endif // DSDETACHEDWINDOW_H
