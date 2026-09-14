#include "DSSidebarStore.h"

#include "Application.h" // CApplication::configDir()（配置目录：文档/PocketWeb/configure）

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
const QString kSidebarJsonFile = QStringLiteral("sidebar_applets.json");
const QString kAppletsKey = QStringLiteral("applets");

// 配置目录：文档/PocketWeb/configure
// 与 CApplication::configDir() 使用同一规则；app 实例不可用时按同样规则兜底。
QString configureDir()
{
    if (CApplication *app = CApplication::instance()) {
        const QString dir = app->configDir();
        if (!dir.isEmpty()) {
            return dir;
        }
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + QStringLiteral("/PocketWeb/configure");
}
} // namespace

QString CDSSidebarStore::jsonPath()
{
    return configureDir() + QLatin1Char('/') + kSidebarJsonFile;
}

QStringList CDSSidebarStore::load()
{
    QStringList names;
    const QString path = jsonPath();
    // 文件不存在则不读取（还没附加过任何小程序）
    if (!QFile::exists(path)) {
        return names;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return names;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return names;
    }

    const QJsonArray arr = doc.object().value(kAppletsKey).toArray();
    for (const QJsonValue &value : arr) {
        const QString name = value.toString().trimmed();
        // 手工改坏的记录（空名称 / 重复名称）直接跳过，避免侧边栏出现空按钮
        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

bool CDSSidebarStore::save(const QStringList &names)
{
    const QString path = jsonPath();
    // 目录不存在则自动创建
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const QString &name : names) {
        if (!name.trimmed().isEmpty()) {
            arr.append(name);
        }
    }
    QJsonObject root;
    root.insert(kAppletsKey, arr);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}
