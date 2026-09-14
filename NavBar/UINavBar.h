#ifndef UINAVBAR_H
#define UINAVBAR_H

#include <QWidget>

class CUINavBarItem;
class QIcon;

// 整体导航工作区：左侧导航栏（右侧内容区由 CDSPocketWebWindow 提供）。
// PocketWeb 版已把原「第 2 部分树形菜单」「第 3 部分底部按钮列」整块去掉，
// 只保留第 0 部分信息区、第 1 部分按钮列与固定的展开/收起按钮。
class CUINavBar : public QWidget
{
    Q_OBJECT

public:
    explicit CUINavBar(QWidget *parent = nullptr);
    ~CUINavBar() override;

    // 左侧导航栏
    CUINavBarItem *navBar() const;

    // 给第 1 部分增加一个按钮（图标随机，每次启动重新随机），
    // 返回与该按钮对应的唯一 ID（系统随机生成，不重复）
    QString AddTopBtn(const QString &title);
    // 根据 AddTopBtn 返回的 ID 获取按钮对应的标题（无效 ID 返回空字符串）
    QString GetTopBtnTitle(const QString &id) const;
    // 设置 / 删除第 1 部分按钮（透传 CUINavBarItem，说明见 UINavBarItem.h）
    void SetTopBtnIcon(const QString &id, const QIcon &icon);
    void RemoveTopBtn(const QString &id);

    // ---- 第 0 部分（任务属性：图标 + 三行文本）信息接口 ----
    void SetInfoIcon(const QIcon &icon);   // 设置第 0 部分的图标
    void SetInfoCode(const QString &code); // 设置第 1 行文本 InfoCode
    QString GetInfoCode() const;
    void SetInfoName(const QString &name); // 设置第 2 行文本 InfoName
    QString GetInfoName() const;
    void SetInfoText(const QString &text); // 设置第 3 行文本 InfoText
    QString GetInfoText() const;

signals:
    // 鼠标左键单击第 0 部分图标时发出
    void infoClicked();
    // 鼠标右键单击第 0 部分图标时发出
    void infoRightClicked();
    // 鼠标左键点击第 1 部分按钮时发出（参数为该按钮的 ID，仅第 1 部分按钮有效）
    void topbtnClicked(const QString &id);
    // 鼠标右键点击第 1 部分按钮时发出（id 为按钮 ID，pos 为全局坐标）
    void topBtnContextMenuRequested(const QString &id, const QPoint &pos);

private:
    CUINavBarItem *m_navBar = nullptr; // 左侧导航栏
};

#endif // UINAVBAR_H
