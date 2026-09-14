#ifndef DSSIDEBARSTORE_H
#define DSSIDEBARSTORE_H

#include <QString>
#include <QStringList>

// 侧边栏「已附加的小程序」清单存取（全部静态方法，无状态）：
//   文件  文档/PocketWeb/configure/sidebar_applets.json
//   内容  {"applets":["名称1","名称2"]}  —— 只存名称，按附加先后顺序
//
// 【为什么单独存一份，而不是写进 webapplets.json】
// webapplets.json 的数据结构（DSWebApplet）与读写代码（CDSWebAppletStore）保持与上游
// 逐字一致；「在侧边栏显示」只是宿主侧的显示偏好，因此独立成文件，不动小程序的数据格式。
//
// 【按名称关联的后果】
// 清单里存的是名称，所以小程序被改名后旧名称匹配不上，该项会在下一次同步时被
// 自动剔除（重新附加即可）；小程序被删除时同样会被剔除 —— 这正是需求要的行为。
class CDSSidebarStore
{
public:
    // JSON 文件路径（文档/PocketWeb/configure/sidebar_applets.json）
    static QString jsonPath();

    // 读取清单：文件不存在 / 内容不合法 → 空列表；空名称与重复名称会被跳过
    static QStringList load();
    // 保存清单（目录不存在会自动创建）
    static bool save(const QStringList &names);
};

#endif // DSSIDEBARSTORE_H
