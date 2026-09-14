#include "DSWebEngineView.h"

#ifdef DSH_HAVE_WEBENGINE

#include <QAction>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QMenu>
#include <QUrl>
#include <QWebEngineContextMenuRequest>

CDSWebEngineView::CDSWebEngineView(QWidget *parent)
    : QWebEngineView(parent)
{
}

void CDSWebEngineView::contextMenuEvent(QContextMenuEvent *event)
{
    // 标准右键菜单（含“在新标签页中打开链接”“复制链接地址”等 Qt 内置项）
    QMenu *menu = createStandardContextMenu();
    if (!menu) {
        event->accept();
        return;
    }

    // 需求：**无论右键落在链接、按钮还是空白处**，菜单里都要有“用默认浏览器打开”这一项 ——
    // 有些页面（例如带视频的站点）在内嵌窗口里受限于编解码器放不出来，丢给系统默认浏览器
    // （Edge / Chrome）就能正常看；而页面上很多按钮并不是链接，从 linkUrl 取不到地址。
    //   右键点在链接上 → 打开该链接；
    //   否则          → 打开当前页面。
    const QUrl linkUrl = lastContextMenuRequest()
                             ? lastContextMenuRequest()->linkUrl()
                             : QUrl();
    const bool onLink = linkUrl.isValid() && !linkUrl.isEmpty();

    QAction *openDefault = new QAction(
        onLink ? QStringLiteral("使用默认浏览器打开链接")
               : QStringLiteral("使用默认浏览器打开此页面"), menu);
    connect(openDefault, &QAction::triggered, this, [this, linkUrl, onLink]() {
        // 按用户要求走“系统默认浏览器”（不走内嵌窗口、也不强制 Edge）
        QDesktopServices::openUrl(onLink ? linkUrl : url());
    });

    // 放在菜单最前面，并用一条分隔线与 Qt 内置项隔开，方便一眼看到
    QAction *first = menu->actions().isEmpty() ? nullptr : menu->actions().constFirst();
    menu->insertAction(first, openDefault);
    if (first) {
        menu->insertSeparator(first);
    }

    menu->exec(event->globalPos());
    delete menu;
    event->accept();
}

#endif // DSH_HAVE_WEBENGINE
