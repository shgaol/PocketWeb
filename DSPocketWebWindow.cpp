#include "DSPocketWebWindow.h"

#include "DSDetachedWindow.h" // CDSDetachedWindow：从标签栏拖出的网页独立窗口
#include "DSMdiArea.h" // CDSMdiArea：空状态下隐藏内部标签栏（见 DSMdiArea.h 说明）
#include "DSSidebarStore.h" // CDSSidebarStore：侧边栏「已附加的小程序」清单
#include "NavBar/UINavBar.h" // CUINavBar：左侧导航栏
#include "NavBar/UINavBarItem.h" // CUINavBarItem::makeLetterIcon（名称首字兜底图标）
#include "DSWebViewWindow.h"
#include "WebApplet/DSWebAppletPage.h"
#include "WebApplet/DSWebAppletStore.h" // 小程序图标（作为 MDI tab 图标的兜底）

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QPixmap>
#include <QResizeEvent>
#include <QSize>
#include <QStyle>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QUrl>
#include <QWindow>
#ifdef DSH_HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineView>
#endif

QIcon makePocketWebIcon(const QColor &bg, int size)
{
    return CUINavBarItem::makeLetterIcon(QLatin1Char('P'), bg, size);
}

namespace {
// 兜底图标底色（与“网页小程序”表页里的字母图标同色）
const QColor kAppletIconColor(0x25, 0x63, 0xEB);

// 把网页窗口的 favicon 补存成小程序图标时的落盘尺寸
// （与 WebApplet/DSWebAppletIconFetcher 里 kIconSize 保持一致）
const int kSavedAppletIconSize = 64;

// 小程序窗口刚打开时的 tab 图标：
//   1) 已保存过该网页的图标（webapplets/icons/<uuid>.png）→ 直接用它；
//   2) 还没取到图标（或图标文件丢了）→ 用名称首字图标兜底，保证 tab 上一定有图标。
// 之后网页自己的 favicon 解出来后（CDSWebViewWindow::faviconChanged）会替换成它，
// 最终 tab 图标与网页图标一致。
QIcon appletTabIcon(const QString &name, const QString &url)
{
    const QList<DSWebApplet> applets = CDSWebAppletStore::load();
    for (const DSWebApplet &applet : applets) {
        if (applet.name != name || applet.url != url) {
            continue; // 名称与网址都一致才算同一个快捷方式
        }
        const QString path = CDSWebAppletStore::iconPath(applet.iconFile);
        if (!path.isEmpty() && QFile::exists(path)) {
            const QIcon icon(path);
            if (!icon.isNull()) {
                return icon;
            }
        }
        break;
    }

    QString first = name.left(1);
    if (first.isEmpty()) {
        first = QStringLiteral("W"); // 名称异常为空时的兜底字（与表页一致）
    }
    return CUINavBarItem::makeLetterIcon(first.at(0), kAppletIconColor);
}

// 侧边栏里小程序按钮的图标：已抓到的网页图标优先，否则用名称首字图标兜底
//（与表页里的 CDSWebAppletPage::appletIcon() 规则一致）
QIcon sidebarAppletIcon(const DSWebApplet &applet)
{
    const QString path = CDSWebAppletStore::iconPath(applet.iconFile);
    if (!path.isEmpty() && QFile::exists(path)) {
        const QIcon icon(path);
        if (!icon.isNull()) {
            return icon;
        }
    }
    QString first = applet.name.left(1);
    if (first.isEmpty()) {
        first = QStringLiteral("W"); // 名称异常为空时的兜底字（与表页一致）
    }
    return CUINavBarItem::makeLetterIcon(first.at(0), kAppletIconColor);
}

#ifdef DSH_HAVE_WEBENGINE
// 让 root 里所有网页视图重新合成一帧。
// 背景：QWebEngineView 内部是 QQuickWidget（离屏渲染到 FBO），而 QMdiArea 的 TabbedView
// 只显示当前子窗口 —— 切换标签时其它子窗口会被隐藏，Qt 在 QWebEngineView::hideEvent 里
// 会把 page 置为不可见（qwebengineview.cpp），渲染随之暂停；切回来时不一定恢复。
// 做法：恢复 page 可见性并解除冻结，再 update() + 1px 尺寸微调强制走一次 resize，
// 逼 WebEngine 重新合成。
// 注意：这里刻意不做 hide()/show()（不做表面重建）—— 反复重建渲染表面会让“切几次标签后
// 表面失效”来得更快；表面真失效时由网页窗口自己的「刷新」按钮走
// CDSWebViewWindow::rebuildView() 重建视图。
void kickWebEngineRenders(QWidget *root)
{
    if (!root) {
        return;
    }
    const QList<QWebEngineView *> views = root->findChildren<QWebEngineView *>();
    for (QWebEngineView *view : views) {
        if (QWebEnginePage *page = view->page()) {
            page->setVisible(true);
            page->setLifecycleState(QWebEnginePage::LifecycleState::Active);
        }
        view->update();
        const QSize size = view->size();
        if (size.isEmpty()) {
            continue;
        }
        view->resize(size.width() + 1, size.height());
        view->resize(size);
    }
}
#endif
} // namespace

CDSPocketWebWindow::CDSPocketWebWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 中央容器：左侧 CUINavBar 导航栏 + 右侧 QTabWidget（网页小程序 / 网页 两个页签）
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 左侧：导航栏（宽度由 updateNavBarWidth 控制：展开 = 窗口宽 1/6，收起 = 72px）
    m_navigatorBar = new CUINavBar(central);
    layout->addWidget(m_navigatorBar);

    // 右侧：QTabWidget（两个固定页签）
    m_tabWidget = new QTabWidget(central);
    m_tabWidget->setDocumentMode(true);

    // ---- 页签 1「网页小程序」：小程序表页直接作为页签内容（不再放进 MDI）----
    // 表页常驻创建：数据来自 JSON，增加/修改/删除都在表页里完成；
    // 页签本身不提供关闭按钮，所以不存在“关掉再打开”导致数据丢失的问题。
    m_appletPage = new CDSWebAppletPage(m_tabWidget);
    // 表页里双击小程序 → 在「网页」页签的 MDI 中打开对应网页（按小程序名称去重）
    connect(m_appletPage, &CDSWebAppletPage::openRequested,
            this, &CDSPocketWebWindow::openWebAppletWindow);
    // 表页右键「附加到侧边栏」→ 记进侧边栏清单并重建按钮
    connect(m_appletPage, &CDSWebAppletPage::attachToSidebarRequested,
            this, &CDSPocketWebWindow::attachAppletToSidebar);
    // 表页里的小程序列表变了 → 重建侧边栏（被删除的小程序，其侧边栏项一并去掉）
    connect(m_appletPage, &CDSWebAppletPage::appletsChanged,
            this, &CDSPocketWebWindow::refreshSidebarApplets);
    m_tabWidget->addTab(m_appletPage, QStringLiteral("网页小程序"));

    // ---- 页签 2「网页」：MDI 区域（Tab 模式、带关闭按钮、Fusion 主题）----
    m_appMdiArea = new CDSMdiArea(m_tabWidget);
    m_appMdiArea->setViewMode(QMdiArea::TabbedView); // Tab 模式
    // 开启 Tab 关闭按钮：点击 × 时 QMdiArea 内部自动关闭对应子窗口
    // （子窗口设置了 WA_DeleteOnClose，关闭后 destroyed 信号会清理映射）
    m_appMdiArea->setTabsClosable(true);
    // 子窗口尺寸由我们自己管（每个窗口都用 showMaximized() 打开），
    // 关掉“激活时自动最大化/还原”这一步：它会随每次切标签对子窗口做一次
    // showNormal()/showMaximized()，等于给里面的网页视图又多加一轮隐藏/显示与缩放，
    // 而网页视图（QWebEngineView）恰恰最怕这种反复折腾。
    m_appMdiArea->setOption(QMdiArea::DontMaximizeSubWindowOnActivation, true);
    // Fusion 主题（只作用于该 MDI 区域及其子窗口，不影响程序其他部分外观）
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        fusion->setParent(m_appMdiArea); // 生命周期交给 MDI 区域管理
        m_appMdiArea->setStyle(fusion);
    }
    // 限制 MDI 内部 tab 栏大小：tab 高度压缩到接近标题栏，紧凑显示；
    // tab 宽度与标题文本长度一致（qproperty-expanding:false 关闭拉伸，
    // 去掉固定 min-width，宽度由标题文本 + padding 决定）
    // （QMdiArea 的内部 QTabBar 无公开访问器，用 QSS 后代选择器命中）
    m_appMdiArea->setStyleSheet(QStringLiteral(
        "QMdiArea QTabBar {"
        "  min-height: 26px;"
        "  max-height: 26px;"
        "  qproperty-expanding: false;"
        "}"
        "QMdiArea QTabBar::tab {"
        "  height: 22px;"
        "  padding: 0px 8px;"
        "}"
        "QMdiArea QTabBar::close-button {"
        "  margin: 2px;"
        "}"));

#ifdef DSH_HAVE_WEBENGINE
    // 切换 MDI 标签时，让当前网页视图重新合成一帧，避免个别网站停在空白画面上
    // （原因见上面的 kickWebEngineRenders）。
    // 延到事件循环下一轮执行：此时 MDI 已经把子窗口摆好，且避开激活流程中的重入。
    // 说明：这里用排队的元对象调用（Qt::QueuedConnection）而不是定时器 —— 语义等价
    // （都是把动作推到事件循环下一轮），但不引入 QTimer（需求：非必要不用定时器）。
    connect(m_appMdiArea, &QMdiArea::subWindowActivated, this,
            [this](QMdiSubWindow *) {
                QMetaObject::invokeMethod(this, [this]() {
                    if (QMdiSubWindow *active = m_appMdiArea->activeSubWindow()) {
                        kickWebEngineRenders(active->widget());
                    }
                }, Qt::QueuedConnection);
            });
    // 从「网页小程序」页签切回「网页」页签时，同样让当前网页视图重新合成一帧：
    // 网页视图被隐藏后不一定恢复合成（原因见上面的 kickWebEngineRenders），
    // 表现就是切回来一片空白、刷新也刷不出来。
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int) {
        if (m_tabWidget->currentWidget() != m_appMdiArea) {
            return; // 只看“切回网页页签”
        }
        QMetaObject::invokeMethod(this, [this]() {
            if (QMdiSubWindow *active = m_appMdiArea->activeSubWindow()) {
                kickWebEngineRenders(active->widget());
            }
        }, Qt::QueuedConnection);
    });
#endif

    // 标签被拖出标签栏（由 CDSMdiArea 识别）→ 变成独立窗口；
    // 独立窗口被拖回并在本区域内松手 → 重新停靠（见 detachWebApplet / dockDetachedWebApplet）
    connect(m_appMdiArea, &CDSMdiArea::tabDragOutRequested,
            this, &CDSPocketWebWindow::detachWebApplet);

    m_tabWidget->addTab(m_appMdiArea, QStringLiteral("网页"));

    // 启动默认显示「网页小程序」页签（索引 0）；main.cpp 里还会显式调用
    // openWebApplets() 再确认一次。
    m_tabWidget->setCurrentIndex(0);

    layout->addWidget(m_tabWidget, 1); // 右侧占满剩余空间
    setCentralWidget(central);

    // ---- 左侧导航栏第 1 部分：只放「已附加到侧边栏」的小程序按钮 ----
    // 「网页小程序」表页已经是右侧 QTabWidget 的第 1 个页签，侧边栏里不再重复放按钮
    connect(m_navigatorBar, &CUINavBar::topbtnClicked,
            this, &CDSPocketWebWindow::onNavTopBtnClicked);
    connect(m_navigatorBar, &CUINavBar::topBtnContextMenuRequested,
            this, &CDSPocketWebWindow::onNavTopBtnContextMenu);
    refreshSidebarApplets();

    // 导航栏展开/收起时同步宽度（展开 = 窗口宽 1/6，收起 72px）
    connect(m_navigatorBar->navBar(), &CUINavBarItem::expandedChanged,
            this, [this](bool) { updateNavBarWidth(); });
    updateNavBarWidth();
    setWindowTitle(QStringLiteral("PocketWeb"));
    resize(1100, 700);

    // ---- 系统托盘：关闭/最小化隐藏到托盘，右键「显示 / 退出」 ----
    m_trayMenu = new QMenu(this);
    QAction *showAct = m_trayMenu->addAction(QStringLiteral("显示"));
    QAction *quitAct = m_trayMenu->addAction(QStringLiteral("退出"));
    connect(showAct, &QAction::triggered, this, &CDSPocketWebWindow::showWindow);
    connect(quitAct, &QAction::triggered, this, &CDSPocketWebWindow::quitApplication);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(makePocketWebIcon(QColor(0x25, 0x63, 0xEB))); // 蓝色 P 图标
    m_trayIcon->setToolTip(QStringLiteral("PocketWeb"));
    m_trayIcon->setContextMenu(m_trayMenu);
    // 双击托盘图标 = 「显示」（除右键菜单外再给一个更顺手的入口）
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) {
                    showWindow();
                }
            });
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    }
    // 真正退出时先移除托盘图标, 避免进程退出后图标残留残影
    // (Windows 上常要鼠标移到托盘区才刷新消失)。挂在 aboutToQuit
    // 以覆盖一切合法退出路径(托盘「退出」/ quit() 等)。
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        if (m_trayIcon)
            m_trayIcon->hide();
    });
}

CDSPocketWebWindow::~CDSPocketWebWindow() = default;

CUINavBar *CDSPocketWebWindow::navigatorBar() const
{
    return m_navigatorBar;
}

void CDSPocketWebWindow::openWebApplets()
{
    // 「网页小程序」现在是右侧 QTabWidget 的第 1 个固定页签 —— 表页在窗口构造时
    // 就常驻创建好了，所以这里只把它切到前台，不再新建/激活 MDI 子窗口。
    m_tabWidget->setCurrentWidget(m_appletPage);
}

// 网页小程序窗口：在 MDI 区域中打开小程序网址。
// - **profile（登录信息 + cache）按网址复用**：小程序网址若属于内置站点预设
//   （deepseek.com / toutiao.com / github.com），就用该预设的类型与 profile ——
//   登录信息落在 PocketWeb 自己的 configure 目录下（configure/deepseek-web 等）；
//   其它网址用网页小程序自己的 profile（configure/webapplet-web）；
// - 站内链接（小程序自己的站点，如 chat.deepseek.com → deepseek.com）在窗口内导航，
//   站外链接交给外部浏览器（Edge）打开，右键菜单也有“使用默认浏览器打开链接”；
// - MDI 标题用小程序名称（不跟随网址），按小程序名称去重，重复双击只激活已打开的窗口。
void CDSPocketWebWindow::openWebAppletWindow(const QString &name, const QString &url)
{
    const QString key = QStringLiteral("小程序: ") + name;
    // 该小程序已经打开：可能仍是 MDI 里的一个标签，也可能被拖出成了独立窗口
    if (QWidget *existing = m_webWindows.value(key, nullptr)) {
        if (auto *sub = qobject_cast<QMdiSubWindow *>(existing)) {
            m_tabWidget->setCurrentWidget(m_appMdiArea); // 切到「网页」页签
            m_appMdiArea->setActiveSubWindow(sub);
        }
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    // 网址属于内置站点（DeepSeek / 今日头条 / GitHub）→ 复用该站点类型：
    // profile 与内置站点预设窗口完全相同，登录状态直接沿用，不必再登录一次。
    const CDSWebProfileKind kind = CDSWebViewWindow::kindForUrl(url);
    // 第三个参数：本窗口的“站内域名后缀”（由小程序网址推出），
    // 与内置站点预设里 DeepSeek 用 deepseek.com、今日头条用 toutiao.com 同理
    auto *view = new CDSWebViewWindow(kind, nullptr, CDSWebAppletPage::siteHostSuffix(url));
    QMdiSubWindow *sub = m_appMdiArea->addSubWindow(view);
    sub->setWindowTitle(name); // 表页里的快捷方式名称
    // tab 图标：先放“已保存的网页图标 / 名称首字兜底图标”，保证 tab 上立刻有图标；
    // 网页自己的 favicon 解出来后（下面的 faviconChanged）替换成它 —— 最终 tab 图标与网页图标一致。
    const QIcon fallbackIcon = appletTabIcon(name, url);
    sub->setWindowIcon(fallbackIcon);
    connect(view, &CDSWebViewWindow::faviconChanged, sub, [sub, fallbackIcon](const QIcon &icon) {
        if (icon.cacheKey() == fallbackIcon.cacheKey()) {
            return; // 与当前图标相同：不必重复刷新 tab
        }
        sub->setWindowIcon(icon);
    });
    // 顺手把网页自己解出来的 favicon 补存成这条小程序的图标（只在“还没有图标文件”时写）。
    // 有些站点（例如 chat.deepseek.com）的服务器不返回真正的 favicon 文件，离线抓取必然失败；
    // 但打开一次网页后 Chromium 能解出图标 —— 借它把侧边栏按钮与表页条目的图标补上，
    // 于是侧边栏 / 表页 / MDI 标签三处图标一致（同源于 Chromium 解出的图标）。
    connect(view, &CDSWebViewWindow::faviconChanged, this,
            [this, name, url](const QIcon &icon) {
                saveAppletIconFromWindow(name, url, icon);
            });
    sub->setAttribute(Qt::WA_DeleteOnClose); // 关闭时真正销毁(触发 destroyed 清理映射)
    // 注意：不连接 urlChanged 改标题——小程序窗口标题保持快捷方式名称，不跟随网址。
    view->openUrl(url);
    sub->showMaximized();
    m_webWindows.insert(key, sub);

    connect(sub, &QObject::destroyed, this, [this, key, sub](QObject *) {
        if (m_webWindows.value(key) == sub)
            m_webWindows.remove(key);
    });

    m_tabWidget->setCurrentWidget(m_appMdiArea); // 切到「网页」页签，让刚打开的网页可见
}

// 显示主窗口：托盘「显示」、双击托盘图标、以及「再次启动本程序」（单实例管道）三条路径都走这里。
void CDSPocketWebWindow::showWindow()
{
    // 从最小化/隐藏到托盘的状态恢复显示（保留之前的最大化状态）
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    raise();
    activateWindow();
}

void CDSPocketWebWindow::quitApplication()
{
    // 真正的退出：会触发 aboutToQuit（已挂接隐藏托盘图标）
    QApplication::quit();
}

void CDSPocketWebWindow::closeEvent(QCloseEvent *event)
{
    // 关闭 = 隐藏到托盘，不真正退出（只有托盘菜单「退出」才结束程序）
    hide();
    event->ignore();
}

void CDSPocketWebWindow::changeEvent(QEvent *event)
{
    // 最小化 = 隐藏到托盘。
    // 延到事件循环下一轮再 hide()：在 WindowStateChange 事件处理过程中直接隐藏窗口，
    // 部分 Windows 主题下会留下「最小化动画未完成」的残影。
    // 这里用排队的元对象调用代替定时器（语义等价，且不引入 QTimer）。
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        QMetaObject::invokeMethod(this, [this]() { hide(); }, Qt::QueuedConnection);
    }
    QMainWindow::changeEvent(event);
}

// 「网页」页签里某个标签被拖出标签栏 → 把它变成一个独立窗口。
//
// 本槽是在**标签栏自己的鼠标事件**里被调用的：如果就地移除子窗口，
// QMdiArea::removeSubWindow() 会去改那个标签栏（removeTab），
// 相当于在标签栏处理鼠标事件的过程中改它自己 —— 会造成重入。
// 因此这里排队到事件循环下一轮再动手（排队调用，不引入 QTimer）。
void CDSPocketWebWindow::detachWebApplet(QMdiSubWindow *subWindow)
{
    QMetaObject::invokeMethod(this, [this, subWindow]() { detachWebAppletNow(subWindow); },
                              Qt::QueuedConnection);
}

void CDSPocketWebWindow::detachWebAppletNow(QMdiSubWindow *subWindow)
{
    // 排队期间它可能已经被关掉或移走了
    if (!subWindow || !m_appMdiArea->subWindowList().contains(subWindow)) {
        return;
    }

    // 找到它对应的去重键（值可能是 MDI 子窗口，也可能是已经拖出的独立窗口）
    QString key;
    for (auto it = m_webWindows.constBegin(); it != m_webWindows.constEnd(); ++it) {
        if (it.value() == subWindow) {
            key = it.key();
            break;
        }
    }
    if (key.isEmpty()) {
        return; // 不在去重表里（不属于任何小程序）：不处理
    }

    QWidget *content = subWindow->widget();
    if (!content) {
        return;
    }
    const QString title = subWindow->windowTitle();
    const QIcon icon = subWindow->windowIcon();

    // 从 MDI 区域摘出来：removeSubWindow() 只把它移出 childWindows 并把父设为 nullptr
    // （qmdiarea.cpp:2003-2014，并不删除），再把网页从子窗口上取走，最后销毁空壳子窗口。
    m_appMdiArea->removeSubWindow(subWindow);
    subWindow->setWidget(nullptr);
    subWindow->deleteLater();

    auto *detached = new CDSDetachedWindow(m_appMdiArea, content, title, icon);
    // 拖动经过「网页」页签区域 → 显示/隐藏停靠高亮边框
    connect(detached, &CDSDetachedWindow::dockHintChanged,
            m_appMdiArea, &CDSMdiArea::setDockHintVisible);
    // 拖回并在区域内松手 → 重新停靠（key 在这里捕获，停靠时要用）
    connect(detached, &CDSDetachedWindow::dockRequested, this, [this, key, detached]() {
        dockDetachedWebApplet(key, detached);
    });
    // 直接关掉独立窗口 = 关掉该网页：销毁时清掉去重表记录
    connect(detached, &QObject::destroyed, this, [this, key, detached](QObject *) {
        if (m_webWindows.value(key) == detached)
            m_webWindows.remove(key);
    });

    m_webWindows.insert(key, detached);

    // 放到光标处，让光标正落在这个新窗口的标题栏上
    const QPoint cursor = QCursor::pos();
    detached->move(cursor - QPoint(60, 12));
    detached->show();

    // 无缝衔接：把接下来的移动交给系统，等价于“用户此刻正按住它的标题栏”。
    // 左键此刻仍按着，所以能接上（Qt 的实现是 ReleaseCapture + PostMessage
    // SC_DRAGMOVE，见 qwindowswindow.cpp:3286）；万一没接上也不影响功能，
    // 用户重新拖一次标题栏即可。
    if (QWindow *handle = detached->windowHandle()) {
        handle->startSystemMove();
    }

#ifdef DSH_HAVE_WEBENGINE
    // 网页视图换了个顶层窗口承载，让它重新合成一帧（不重载页面）
    kickWebEngineRenders(content);
#endif
}

// 独立窗口被拖回「网页」页签区域并松手 → 重新停靠成 MDI 标签：激活对应网页标签并最大化。
void CDSPocketWebWindow::dockDetachedWebApplet(const QString &key, CDSDetachedWindow *window)
{
    if (!window) {
        return;
    }

    // 取回网页：必须用 takeCentralWidget()（它会把部件 setParent(nullptr) 并把所有权
    // 交还调用方，qmainwindow.cpp:565-573）。绝不能用 setCentralWidget(nullptr)
    // —— 那会把原来的中央部件 deleteLater() 掉（qmainwindow.cpp:551-553），等于把网页销毁。
    QWidget *content = window->takeCentralWidget();
    if (!content) {
        return;
    }
    const QString title = window->windowTitle();
    const QIcon icon = window->windowIcon();

    // 先断开本窗口与它的连接（尤其那条 destroyed 清理）：去重表接下来要指向新的 MDI 子窗口
    window->disconnect(this);

    QMdiSubWindow *sub = m_appMdiArea->dockBack(content, title, icon);
    if (!sub) {
        window->deleteLater();
        return;
    }

    m_webWindows.insert(key, sub);
    connect(sub, &QObject::destroyed, this, [this, key, sub](QObject *) {
        if (m_webWindows.value(key) == sub)
            m_webWindows.remove(key);
    });

    // 需求：拖回时自动激活网页标签（页签切回「网页」，dockBack 里已激活该标签并最大化）
    m_tabWidget->setCurrentWidget(m_appMdiArea);
    m_appMdiArea->setActiveSubWindow(sub);

    // 空壳独立窗口收尾（网页已经被交还给 MDI 了）
    window->hide();
    window->deleteLater();

#ifdef DSH_HAVE_WEBENGINE
    kickWebEngineRenders(content);
#endif
}

// ---- 左侧导航栏 ----

void CDSPocketWebWindow::updateNavBarWidth()
{
    const bool expanded = m_navigatorBar->navBar()->isExpanded();
    // 展开时导航栏宽度 = 主窗口宽度的 1/6；收起时固定 72px
    m_navigatorBar->setFixedWidth(expanded ? width() / 6 : 72);
}

void CDSPocketWebWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateNavBarWidth();
}

// ---- 侧边栏里的小程序按钮（“附加到侧边栏”）----

// 按清单重建侧边栏里的小程序按钮，并把最新清单同步给表页。
// 清单里已经不存在的小程序（被删除 / 改名）会顺手从清单里剔除 —— 这就满足了
// “网页小程序被删除，对应的侧边栏项也删除”，而且离线删除的情况在下次启动时也会被清掉。
void CDSPocketWebWindow::refreshSidebarApplets()
{
    const QList<DSWebApplet> applets = CDSWebAppletStore::load();
    QHash<QString, DSWebApplet> byName;
    for (const DSWebApplet &applet : applets) {
        byName.insert(applet.name, applet);
    }

    const QStringList stored = CDSSidebarStore::load();
    QStringList valid;
    for (const QString &name : stored) {
        if (byName.contains(name)) {
            valid.append(name);
        }
    }
    if (valid != stored) {
        CDSSidebarStore::save(valid); // 剔除已失效的名称并落盘
    }

    // 数量很少，整体重建最简单，按钮顺序也永远与清单一致
    for (const QString &id : m_sidebarIds) {
        m_navigatorBar->RemoveTopBtn(id);
    }
    m_sidebarIds.clear();

    for (const QString &name : valid) {
        const QString id = m_navigatorBar->AddTopBtn(name);
        m_navigatorBar->SetTopBtnIcon(id, sidebarAppletIcon(byName.value(name)));
        m_sidebarIds.insert(name, id);
    }

    // 表页据此把右键菜单里的「附加到侧边栏」置灰
    m_appletPage->setSidebarNames(valid);
}

// 网页窗口自己解出 favicon 后调用：若这条小程序还没有图标文件，就把这个图标存下来。
// 只在“缺图标”时才写 —— 已经通过离线抓取拿到图标的条目不会被覆盖，
// 因此“能正常抓取的站点”行为不变，只有抓不到的站点在打开一次后自动补齐。
void CDSPocketWebWindow::saveAppletIconFromWindow(const QString &name, const QString &url,
                                                  const QIcon &icon)
{
    if (icon.isNull()) {
        return; // 还没解出图标：不发/不存（外层继续用兜底图标）
    }

    const QList<DSWebApplet> applets = CDSWebAppletStore::load();
    for (const DSWebApplet &applet : applets) {
        if (applet.name != name || applet.url != url) {
            continue; // 名称与网址都一致才算同一条
        }

        // 已经有图标文件 → 保持原样（不覆盖正常抓取到的图标）
        if (!applet.iconFile.isEmpty()
            && QFile::exists(CDSWebAppletStore::iconPath(applet.iconFile))) {
            return;
        }

        // 没有图标文件名就现起一个（JSON 被手工改坏的情况）
        QString iconFile = applet.iconFile;
        if (iconFile.isEmpty()) {
            iconFile = CDSWebAppletStore::makeIconFileName();
        }
        const QString path = CDSWebAppletStore::iconPath(iconFile);
        if (path.isEmpty()) {
            return;
        }

        QDir().mkpath(CDSWebAppletStore::iconsDir());
        const QPixmap pixmap = icon.pixmap(QSize(kSavedAppletIconSize, kSavedAppletIconSize));
        if (pixmap.isNull() || !pixmap.save(path, "PNG")) {
            return; // 落盘失败：什么都不改，界面继续用兜底图标
        }

        CDSWebAppletStore::setIconFile(name, url, iconFile); // 写回 JSON
        // 表页那一行 + 侧边栏按钮一起换成新图标
        m_appletPage->refreshIcons();
        refreshSidebarApplets();
        return;
    }
}

void CDSPocketWebWindow::attachAppletToSidebar(const QString &name, const QString &url)
{
    Q_UNUSED(url); // 清单只存名称；打开时按名称回查当前网址
    QStringList names = CDSSidebarStore::load();
    if (!names.contains(name)) {
        names.append(name);
        CDSSidebarStore::save(names);
    }
    refreshSidebarApplets();
}

void CDSPocketWebWindow::removeAppletFromSidebar(const QString &name)
{
    QStringList names = CDSSidebarStore::load();
    if (names.removeAll(name) > 0) {
        CDSSidebarStore::save(names);
    }
    refreshSidebarApplets();
}

void CDSPocketWebWindow::onNavTopBtnClicked(const QString &id)
{
    // 侧边栏第 1 部分里只有「已附加到侧边栏」的小程序按钮：
    // 按名称回查网址后打开（与双击表页里的条目等价）
    for (auto it = m_sidebarIds.constBegin(); it != m_sidebarIds.constEnd(); ++it) {
        if (it.value() != id) {
            continue;
        }
        const QList<DSWebApplet> applets = CDSWebAppletStore::load();
        for (const DSWebApplet &applet : applets) {
            if (applet.name == it.key()) {
                openWebAppletWindow(applet.name, applet.url);
                return;
            }
        }
        return; // 清单里有、小程序却已经没了：下一次同步会把它剔除
    }
}

void CDSPocketWebWindow::onNavTopBtnContextMenu(const QString &id, const QPoint &pos)
{
    // 只对“已附加的小程序”按钮弹菜单（pos 是全局坐标，直接交给 QMenu::exec）
    QString name;
    for (auto it = m_sidebarIds.constBegin(); it != m_sidebarIds.constEnd(); ++it) {
        if (it.value() == id) {
            name = it.key();
            break;
        }
    }
    if (name.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction *removeAct = menu.addAction(QStringLiteral("移除"));
    if (menu.exec(pos) == removeAct) {
        removeAppletFromSidebar(name);
    }
}
