#ifndef DSWEBAPPLETPAGE_H
#define DSWEBAPPLETPAGE_H

#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "DSWebAppletStore.h" // DSWebApplet（小程序条目）

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPoint;

// “网页小程序”表页（PocketWeb 主窗口 QTabWidget 的第 1 个页签内容）：
//   工具栏：增加 / 修改 / 删除
//   快捷方式区：图标 + 名称（图标模式，自动换行，放不下出滚动条）
//   交互：双击打开对应网页；右键某个小程序弹出菜单（修改 / 删除）
//   数据：保存后异步读取该网页的图标，取到后刷新显示（见 CDSWebAppletIconFetcher）
//   持久化：文档/PocketWeb/configure/webapplets/webapplets.json（见 CDSWebAppletStore）
class CDSWebAppletPage : public QWidget
{
    Q_OBJECT

public:
    explicit CDSWebAppletPage(QWidget *parent = nullptr);
    ~CDSWebAppletPage() override;

    // 由小程序网址推出“站内域名后缀”：https://chat.deepseek.com/ → deepseek.com、
    // https://www.163.com/ → 163.com、https://www.abc.com.cn/ → abc.com.cn。
    // 用途与 kSiteInfos 里 DeepSeek 传 "deepseek.com"、今日头条传 "toutiao.com" 相同：
    // 打开小程序窗口时站内链接在窗口内导航、站外链接交给 Edge（见 MainWindow::openWebAppletWindow）。
    static QString siteHostSuffix(const QString &url);

    // 由宿主告知：哪些小程序已经附加到侧边栏。
    // 右键菜单据此决定「附加到侧边栏」是否可用（已附加则置灰；
    // 取消附加在侧边栏自己的右键菜单里，见 DSPocketWebWindow）。
    void setSidebarNames(const QStringList &names);

    // 重新读一遍各条目的 iconFile 并刷新列表里的图标。
    // 用途：图标文件可能在别处被更新（例如网页窗口把 favicon 补存下来之后），
    // 需要让表页立刻显示成新图标。本方法只刷新显示，不会重新发起图标抓取。
    void refreshIcons();

signals:
    // 双击（或在选中项上回车）：请求在 MDI 中打开该网页小程序
    void openRequested(const QString &name, const QString &url);
    // 右键菜单选择「附加到侧边栏」：请求把该小程序加到侧边栏
    void attachToSidebarRequested(const QString &name, const QString &url);
    // 表页里的小程序列表发生了变化（增加 / 修改 / 删除）：
    // 宿主据此同步侧边栏（已不存在的小程序，其侧边栏项要一并去掉）
    void appletsChanged();

private slots:
    // “增加”：弹录入对话框 → 保存 JSON → 读取网页图标
    void onAdd();
    // “修改”（工具栏按钮 / 右键菜单）：弹录入对话框（名称不重复、名称与网址必填）
    void onModify();
    // “删除”（工具栏按钮 / 右键菜单）：确认后删除选中的小程序
    void onDelete();
    // 双击/回车打开选中的小程序
    void onActivated(QListWidgetItem *item);
    // 右键菜单：修改 / 删除 / 附加到侧边栏
    void onContextMenu(const QPoint &pos);
    // 网页图标读取完成：刷新对应条目的图标（JSON 已由读取器写回）
    void onIconReady(const QString &name, const QString &url, const QString &iconFile);

private:
    // 从 JSON 重新载入并重建列表
    void reload();
    // 保存全部小程序到 JSON
    void saveAll() const;
    // 当前选中行（未选中返回 -1）
    int currentRow() const;
    // 生成列表项（图标 + 名称 + 提示）
    QListWidgetItem *makeItem(const DSWebApplet &applet) const;
    // 条目图标：有网页图标用它，否则用名称首字图标兜底
    QIcon appletIcon(const DSWebApplet &applet) const;
    // 刷新某一行的图标/名称/提示
    void refreshItem(int row);
    // 已占用名称（excludeRow 为要排除的行，修改时排除自己；-1 表示不排除）
    QStringList nameListExcept(int excludeRow) const;
    // 启动图标读取（读取器挂在 qApp 下，表页关闭后仍会完成并写回 JSON）
    void fetchIcon(const DSWebApplet &applet);

    QListWidget *m_list = nullptr;   // 快捷方式列表（图标模式）
    QLabel *m_hintLabel = nullptr;   // 底部提示行
    QList<DSWebApplet> m_applets;    // 当前全部小程序（与列表行一一对应）
    QStringList m_sidebarNames;      // 已经附加到侧边栏的小程序名称（由宿主同步）
};

#endif // DSWEBAPPLETPAGE_H
