#ifndef UINAVBARITEM_H
#define UINAVBARITEM_H

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QPoint>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QEvent;
class QLabel;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

// 左侧导航栏（PocketWeb 版：在原 CUINavBarItem 基础上裁剪过）：
//   0. 信息区：图标在上 + 三行文字在下（SetInfoIcon / SetInfoCode / SetInfoName / SetInfoText）
//   1. 按钮列（可滚动）：由外部 AddTopBtn() 动态添加 —— 本工程放的是
//      「已附加到侧边栏」的网页小程序按钮；一个按钮都没有时整列隐藏
//   固定按钮：底部的展开/收起按钮（始终保留）
// 原版还有「第 2 部分树形菜单」与「第 3 部分底部按钮列」，本工程用不到，已整块去掉。
// 展开时按钮显示图标 + 文字并占较宽栏宽；收起时是 72px 的图标窄条。
class CUINavBarItem : public QWidget
{
    Q_OBJECT

public:
    // 内置图标类型（程序内绘制，无需外部图片资源）
    enum class Icon {
        Chat,         // 对话
        Tasks,        // 任务
        Agents,       // 代理
        Jobs,         // 作业
        Skills,       // 技能
        Workflow,     // 工作流
        Grid,         // 分组/栏目（工作区、系统）
        Settings,     // 设置
        Home,         // 首页
        Search,       // 搜索
        Folder,       // 文件夹
        File,         // 文件
        Heart,        // 收藏
        Bell,         // 通知
        Clock,        // 时间
        Mail,         // 邮件
        MapPin,       // 位置
        Send,         // 发送
        Zap,          // 闪电
        Lock,         // 锁定
        Plus,         // 加号
        Minus,        // 减号
        Check,        // 对勾
        Close,        // 关闭
        Info,         // 信息
        Question,     // 疑问
        Edit,         // 编辑
        Trash,        // 删除
        Refresh,      // 刷新
        Download,     // 下载
        Upload,       // 上传
        Copy,         // 复制
        Calendar,     // 日历
        Flag,         // 旗帜
        Camera,       // 相机
        Power,        // 电源
        Sun,          // 太阳
        Moon,         // 月亮
        Cloud,        // 云
        Alert,        // 警告
        Shield,       // 盾牌
        Eye,          // 眼睛
        Key,          // 钥匙
        Tag,          // 标签
        Gift,         // 礼物
        Music,        // 音乐
        Pause,        // 暂停
        Stop,         // 停止
        Battery,      // 电池
        Chart,        // 图表
        Card,         // 银行卡
        Filter,       // 筛选
        ChevronLeft,  // 收起（展开态时显示）
        ChevronRight, // 展开（收起态时显示）
        // A-Z 字母图标（蓝色系，加入随机图标库）
        IconA,        // 字母 A
        IconB,        // 字母 B
        IconC,        // 字母 C
        IconD,        // 字母 D
        IconE,        // 字母 E
        IconF,        // 字母 F
        IconG,        // 字母 G
        IconH,        // 字母 H
        IconI,        // 字母 I
        IconJ,        // 字母 J
        IconK,        // 字母 K
        IconL,        // 字母 L
        IconM,        // 字母 M
        IconN,        // 字母 N
        IconO,        // 字母 O
        IconP,        // 字母 P
        IconQ,        // 字母 Q
        IconR,        // 字母 R
        IconS,        // 字母 S
        IconT,        // 字母 T
        IconU,        // 字母 U
        IconV,        // 字母 V
        IconW,        // 字母 W
        IconX,        // 字母 X
        IconY,        // 字母 Y
        IconZ,        // 字母 Z
    };

    // 生成字母图标：指定颜色的圆角方块背景 + 白色加粗字母（程序内绘制；默认 64px）
    static QIcon makeLetterIcon(const QChar &letter, const QColor &bg, int size = 64);

    explicit CUINavBarItem(QWidget *parent = nullptr);
    ~CUINavBarItem() override;

    int currentIndex() const;     // 当前选中图标按钮序号，-1 表示无
    QString currentTitle() const; // 当前选中图标按钮标题
    int itemCount() const;        // 图标按钮数量（含“设置”）

    // 展开/收起侧边栏
    void setExpanded(bool expanded);
    bool isExpanded() const;

    // ---- 任务属性（第 0 部分：图标 + 三行文本）----
    // 设置第 0 部分的图标
    void SetInfoIcon(const QIcon &icon);
    // 第 1 行文本：InfoCode
    void SetInfoCode(const QString &code);
    QString GetInfoCode() const;
    // 第 2 行文本：InfoName
    void SetInfoName(const QString &name);
    QString GetInfoName() const;
    // 第 3 行文本：InfoText
    void SetInfoText(const QString &text);
    QString GetInfoText() const;

    // ---- 第 1 部分按钮管理 ----
    // 给第 1 部分增加一个按钮：
    //   - 图标从内置图标中随机选取（每次启动重新随机）
    //   - 返回与该按钮对应的唯一 ID（系统随机生成，保证不重复）
    QString AddTopBtn(const QString &title);
    // 根据 AddTopBtn 返回的 ID 获取按钮对应的标题（无效 ID 返回空字符串）
    QString GetTopBtnTitle(const QString &id) const;
    // 设置某个顶部按钮的图标（id 为 AddTopBtn 返回的 ID）：
    // 用于把「已附加到侧边栏」的网页小程序显示成它自己的图标；
    // 传入空 QIcon 则恢复成内置随机图标。
    void SetTopBtnIcon(const QString &id, const QIcon &icon);
    // 删除某个顶部按钮（id 为 AddTopBtn 返回的 ID；ID 无效时忽略）
    void RemoveTopBtn(const QString &id);

signals:
    // 鼠标左键单击第 0 部分图标时发出
    void infoClicked();
    // 鼠标右键单击第 0 部分图标时发出
    void infoRightClicked();
    // 鼠标左键点击第 1 部分按钮时发出（参数为该按钮的 ID，仅第 1 部分按钮有效）
    void topbtnClicked(const QString &id);
    // 鼠标右键点击第 1 部分按钮时发出（参数为该按钮 ID 与全局坐标）：
    // 侧边栏自己的右键菜单（如网页小程序的「移除」）由外部据此弹出
    void topBtnContextMenuRequested(const QString &id, const QPoint &pos);
    // 点击顶部图标按钮时发出（index 从 0 开始）
    void itemClicked(int index, const QString &title);
    // 展开/收起状态变化时发出
    void expandedChanged(bool expanded);

private:
    // 创建一个可滚动的按钮列容器（无边框、透明背景），返回其内部布局
    QScrollArea *createButtonColumn(QVBoxLayout **innerLayout);
    QToolButton *createItemButton(const QString &title, Icon icon);
    void initUi();
    void initStyle();
    void updateInfoIconSize(); // 按当前展开状态设置第 0 部分图标尺寸
    void applyExpandedState(); // 根据 m_expanded 更新按钮与栏宽
    void updateSectionVisibility(); // 第 1 部分：无按钮时隐藏
    void updateButtonState(QToolButton *btn, const QString &title, Icon icon); // 按当前状态刷新单个按钮
    bool eventFilter(QObject *obj, QEvent *event) override; // 捕获第 0 部分图标单击

    QButtonGroup *m_group = nullptr;         // 图标按钮互斥选中组
    QList<QToolButton *> m_buttons;          // 与 m_titles/m_buttonIcons 一一对应
    QStringList m_titles;                    // 各图标按钮标题
    QList<Icon> m_buttonIcons;               // 各图标按钮的图标类型（收起时按蓝色绘制）
    QLabel *m_attrIcon = nullptr;      // 第 0 部分：图标（上）
    QIcon m_infoIcon;                  // 第 0 部分：当前设置的图标（保存以便切换尺寸）
    QLabel *m_infoCodeLabel = nullptr; // 第 0 部分：第 1 行文本 InfoCode
    QLabel *m_infoNameLabel = nullptr; // 第 0 部分：第 2 行文本 InfoName
    QLabel *m_infoTextLabel = nullptr; // 第 0 部分：第 3 行文本 InfoText
    QToolButton *m_toggleButton = nullptr;   // 底部展开/收起按钮
    QVBoxLayout *m_layout = nullptr;         // 主布局
    QScrollArea *m_topColumn = nullptr;      // 第 1 部分按钮列容器（无按钮时隐藏）
    QVBoxLayout *m_topColumnLayout = nullptr;   // 第 1 部分按钮列（可滚动）
    QHash<QString, QToolButton *> m_topButtonIds; // AddTopBtn 返回的 ID -> 按钮
    int m_spacerIndex = -1;                  // 弹性占位在布局中的序号（第 1 部分隐藏时撑开）
    bool m_expanded = false;                 // 当前是否展开
};

#endif // UINAVBARITEM_H
