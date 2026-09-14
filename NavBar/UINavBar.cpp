#include "UINavBar.h"
#include "UINavBarItem.h"

#include <QHBoxLayout>

CUINavBar::CUINavBar(QWidget *parent)
    : QWidget(parent)
{
    // 水平布局：左侧导航栏（右侧内容区由 MainWindow 提供）
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_navBar = new CUINavBarItem(this);
    layout->addWidget(m_navBar);

    // 转发：第 0 部分图标被鼠标左键/右键单击
    connect(m_navBar, &CUINavBarItem::infoClicked, this, &CUINavBar::infoClicked);
    connect(m_navBar, &CUINavBarItem::infoRightClicked, this, &CUINavBar::infoRightClicked);
    // 转发：第 1 部分按钮被左键点击（带 ID）
    connect(m_navBar, &CUINavBarItem::topbtnClicked, this, &CUINavBar::topbtnClicked);
    // 转发：第 1 部分按钮被右键点击（带 ID 与全局坐标）
    connect(m_navBar, &CUINavBarItem::topBtnContextMenuRequested,
            this, &CUINavBar::topBtnContextMenuRequested);
}

CUINavBar::~CUINavBar() = default;

CUINavBarItem *CUINavBar::navBar() const
{
    return m_navBar;
}

QString CUINavBar::AddTopBtn(const QString &title)
{
    return m_navBar->AddTopBtn(title);
}

QString CUINavBar::GetTopBtnTitle(const QString &id) const
{
    return m_navBar->GetTopBtnTitle(id);
}

void CUINavBar::SetTopBtnIcon(const QString &id, const QIcon &icon)
{
    m_navBar->SetTopBtnIcon(id, icon);
}

void CUINavBar::RemoveTopBtn(const QString &id)
{
    m_navBar->RemoveTopBtn(id);
}

void CUINavBar::SetInfoIcon(const QIcon &icon)
{
    m_navBar->SetInfoIcon(icon);
}

void CUINavBar::SetInfoCode(const QString &code)
{
    m_navBar->SetInfoCode(code);
}

QString CUINavBar::GetInfoCode() const
{
    return m_navBar->GetInfoCode();
}

void CUINavBar::SetInfoName(const QString &name)
{
    m_navBar->SetInfoName(name);
}

QString CUINavBar::GetInfoName() const
{
    return m_navBar->GetInfoName();
}

void CUINavBar::SetInfoText(const QString &text)
{
    m_navBar->SetInfoText(text);
}

QString CUINavBar::GetInfoText() const
{
    return m_navBar->GetInfoText();
}
