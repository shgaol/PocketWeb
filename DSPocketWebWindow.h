#ifndef DSPOCKETWEBWINDOW_H
#define DSPOCKETWEBWINDOW_H

#include "DSWebViewWindow.h" // CDSWebViewWindow / CDSWebProfileKind（网页窗口）

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QMainWindow>
#include <QString>

class CDSDetachedWindow;
class CDSMdiArea;
class CDSWebAppletPage;
class CUINavBar;
class QCloseEvent;
class QEvent;
class QMdiSubWindow;
class QMenu;
class QResizeEvent;
class QSystemTrayIcon;
class QTabWidget;

// 生成应用图标：蓝色圆角方块 + 白色 P 字
QIcon makePocketWebIcon(const QColor &bg, int size = 64);

// PocketWeb 主窗口：左侧 CUINavBar 导航栏 + 右侧 QTabWidget（两个固定页签）。
// 程序启动时默认停在「网页小程序」页签。
//   页签 1「网页小程序」：CDSWebAppletPage 表页（增加/修改/删除快捷方式，双击打开网页）；
//   页签 2「网页」      ：CDSMdiArea，网页窗口在这里以 MDI 标签形式打开。
//
// 【侧边栏里的小程序按钮】（“附加到侧边栏”）
//   第 1 部分**只放已附加的小程序按钮**（不再固定放「网页小程序」按钮 —— 那个表页
//   已经是右侧 QTabWidget 的第 1 个页签）。交互：
//     - 表页里右键某个小程序 →「附加到侧边栏」（已附加的会置灰）；
//     - 侧边栏里右键那个小程序按钮 →「移除」；
//     - 点击按钮 = 打开该小程序的网页（按名称去重，与双击表页条目等价）；
//     - 一个都没附加时第 1 部分整列隐藏，侧边栏只剩信息区与底部的展开/收起按钮；
//     - 清单存在 文档/PocketWeb/configure/sidebar_applets.json（只存名称，见 CDSSidebarStore）；
//     - 小程序被删除（或改名）后名称匹配不上，refreshSidebarApplets() 会把它从清单和
//       侧边栏里一并剔除 —— 满足“网页小程序被删除，对应的侧边栏也删除”。
//
// 本类是把 DSH-Environment 的 MainWindow 按“只保留网页小程序”裁剪后的宿主：
//   - 保留：导航栏、MDI 区域、网页小程序表页与网页小程序窗口的打开逻辑、系统托盘；
//   - 移除：DSH 对话窗口、CMD 终端窗口、DSH 源码管理等与本功能无关的部分。
//
// 系统托盘行为：
//   - 最小化或关闭窗口 → 隐藏到右下角通知区（不退出程序）；
//   - 托盘图标右键菜单：「显示」/「退出」，只有「退出」才真正结束程序；
//   - 双击托盘图标 = 「显示」；
//   - 托盘图标由程序内绘制（makePocketWebIcon），不依赖外部图片资源。
//
// 「再次启动本程序即激活已有实例」由 main.cpp 的单实例管道完成：
// 新实例发 "show" → 本窗口的 showWindow() 槽被调用（从最小化/托盘恢复并置前）。
// 因此本程序**同时只能运行一个实例**。
//
// 与“网页小程序”功能直接相关的函数（openWebApplets / openWebAppletWindow /
// appletTabIcon）照搬自 DSH-Environment 的 MainWindow，逻辑未做改动；
// 差别只在宿主结构上：上游把小程序表页也放进 MDI（打开/激活一个“网页小程序”
// 子窗口），本工程把它改成 QTabWidget 的第 1 个固定页签，于是 openWebApplets()
// 由“新建/激活 MDI 子窗口”改为“切到该页签”；appletTabIcon 与
// openWebAppletWindow（窗口创建、按名称去重、favicon 换 tab 图标）逐字一致。
class CDSPocketWebWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CDSPocketWebWindow(QWidget *parent = nullptr);
    ~CDSPocketWebWindow() override;

    // 获取左侧导航工作区（外部据此设置信息区文本、展开/收起等）
    CUINavBar *navigatorBar() const;

    // 切到「网页小程序」页签（QTabWidget 的第 1 个页签）。
    // 表页在窗口构造时就常驻创建，这里不新建、也不激活任何子窗口。
    void openWebApplets();
    // 在 MDI 中打开一个网页小程序窗口（按小程序名称去重，重复则激活已打开的窗口）
    void openWebAppletWindow(const QString &name, const QString &url);

private slots:
    // 显示主窗口：托盘菜单「显示」/ 双击托盘图标 / 单实例管道收到 "show" 都会走这里
    void showWindow();
    // 托盘菜单「退出」：真正结束程序（区别于关闭窗口 = 隐藏到托盘）
    void quitApplication();
    // 「网页」页签里某个标签被拖出标签栏 → 把它变成独立窗口（见实现里的排队说明）
    void detachWebApplet(QMdiSubWindow *subWindow);
    // 侧边栏第 1 部分按钮左键点击（「网页小程序」按钮 / 已附加的小程序按钮）
    void onNavTopBtnClicked(const QString &id);
    // 侧边栏第 1 部分按钮右键点击 → 弹「移除」菜单（只对已附加的小程序按钮）
    void onNavTopBtnContextMenu(const QString &id, const QPoint &pos);

protected:
    // 窗口尺寸变化时同步导航栏宽度（展开 = 窗口宽 1/6，收起 72px）
    void resizeEvent(QResizeEvent *event) override;
    // 关闭 = 隐藏到托盘（不退出程序）
    void closeEvent(QCloseEvent *event) override;
    // 最小化 = 隐藏到托盘
    void changeEvent(QEvent *event) override;

private:
    // 导航栏宽度：展开时 = 主窗口宽度的 1/6，收起时 72px
    void updateNavBarWidth();
    // 按侧边栏清单重建侧边栏里的小程序按钮，并把最新清单同步给表页。
    // 清单里已经不存在的小程序（被删除 / 改名）会顺手从清单里剔除 —— 满足
    // “网页小程序被删除，对应的侧边栏项也删除”。
    void refreshSidebarApplets();
    // 把某个小程序附加到侧边栏（表页右键菜单）
    void attachAppletToSidebar(const QString &name, const QString &url);
    // 从侧边栏移除某个小程序（侧边栏按钮右键菜单「移除」）
    void removeAppletFromSidebar(const QString &name);
    // 网页窗口解出 favicon 后调用：若这条小程序**还没有图标文件**，就把这个图标存下来。
    // 只在“缺图标”时写，不覆盖离线抓取已有成果（见实现里的说明）。
    void saveAppletIconFromWindow(const QString &name, const QString &url, const QIcon &icon);
    // 独立窗口被拖回「网页」页签区域 → 重新停靠成 MDI 标签（激活 + 最大化）
    void dockDetachedWebApplet(const QString &key, CDSDetachedWindow *window);
    // detachWebApplet 的实际动作（延到事件循环下一轮执行，见实现说明）
    void detachWebAppletNow(QMdiSubWindow *subWindow);

    CUINavBar *m_navigatorBar = nullptr;      // 左侧导航栏
    QTabWidget *m_tabWidget = nullptr;        // 页签容器（网页小程序 / 网页）
    CDSWebAppletPage *m_appletPage = nullptr; // 页签 1「网页小程序」：小程序表页（常驻）
    CDSMdiArea *m_appMdiArea = nullptr;       // 页签 2「网页」：MDI 区域（Tab 模式）
    QHash<QString, QString> m_sidebarIds;     // 已附加的小程序名称 → 侧边栏按钮 ID
    // 去重表：小程序名称 → 当前承载者。未被拖出时是 MDI 子窗口，
    // 被拖出成独立窗口后换成那个 CDSDetachedWindow（值类型故为 QWidget *）。
    QHash<QString, QWidget *> m_webWindows;
    QSystemTrayIcon *m_trayIcon = nullptr; // 右下角通知区图标
    QMenu *m_trayMenu = nullptr;           // 托盘右键菜单（显示 / 退出）
};

#endif // DSPOCKETWEBWINDOW_H
