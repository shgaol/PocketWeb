# PocketWeb 项目说明

> 一个把 DSH-Environment 里的「网页小程序」功能独立出来的 Qt6 Widgets 桌面程序。
> 用页签管理快捷方式，用内嵌 Chromium（Qt WebEngine）显示网页，支持标签拖出成独立窗口。
> **不使用 QML**、**不使用智能指针**。

---

## 1. 运行环境与构建

| 项 | 值 |
|---|---|
| Qt | Qt 6.11.2（msvc2022_64） |
| 编译器 | MSVC 2022 |
| 构建系统 | CMake ≥ 3.16（`CMakeLists.txt` 在工程根目录） |
| Qt 调用目录 | `Z:/QTSource/6.11.2/msvc2022_64`（CMake 里作为 `CMAKE_PREFIX_PATH` 默认值） |
| Qt 源码目录 | `Z:/QTSource/6.11.2/Src`（仅供查阅） |
| C++ 标准 | C++17，MSVC 加 `/utf-8`（源码含中文注释） |

依赖的 Qt 模块：`Widgets`、`Network`，以及可选的 `WebEngineWidgets`（探测到就定义 `DSH_HAVE_WEBENGINE`）。
没有 WebEngine 时退化为 `WebView` 后端，再没有就是只读文本占位，保证工程仍可编译。

---

## 2. 构建产物与部署布局

exe 输出到 **`工作目录/<项目名称>-bin`**，本工程即 `PocketWeb-bin/`。CMake 用 `${PROJECT_NAME}-bin` 拼出来，工程改名时目录名自动跟随。

```
PocketWeb-bin/
├─ PocketWeb.exe                  生成的程序
├─ Qt6Core.dll / Qt6Gui.dll / Qt6Widgets.dll / Qt6Network.dll
├─ Qt6WebEngineCore.dll / Qt6WebEngineWidgets.dll / Qt6Svg.dll …
├─ QtWebEngineProcess.exe         WebEngine 辅助进程【必须留在根目录】
├─ qt.conf                        路径配置（由 cmake/organize_resource.cmake 生成）
└─ Resource/
   ├─ plugins/                    Qt 插件（qwindows、imageformats、styles…）
   ├─ translations/               Qt 翻译 + qtwebengine_locales
   ├─ resources/                  WebEngine 数据（icudtl.dat、*.pak…）
   └─ qml/                        WebEngine 运行所需的 QML 模块
```

部署由两条 `POST_BUILD` 完成：

1. **windeployqt**：按依赖分析把 Qt dll 拷到 exe 同目录，插件/翻译用 `--plugindir` / `--translationdir` 直接落到 `Resource/` 下；
2. **`cmake/organize_resource.cmake`**：把 windeployqt 仍留在根目录的 `resources/`、`qml/`、`translations/` 归整进 `Resource/`，并**生成 `qt.conf`**。与 windeployqt 分开注册，保证即使 windeployqt 没跑也一定会生成 qt.conf。

启动时 `main.cpp` 的 `setupResourceDirByWinApi()` 会把工作目录切到 `exe目录/Resource`；`qt.conf` 让 Qt 从 `Resource/` 下找插件、翻译、qml、数据。

> ⚠️ `QtWebEngineProcess.exe` **不能**移进 `Resource/` —— 它依赖 `Qt6WebEngineCore.dll`，Windows 按 exe 所在目录找依赖，移走会导致 WebEngine 初始化失败、程序直接退出。

---

## 3. 界面结构

```
CDSPocketWebWindow (QMainWindow)
└─ centralWidget = QWidget + QHBoxLayout（无间距、无边距）
   │
   ├─ CUINavBar                     左侧导航栏
   │   └─ CUINavBarItem
   │      ├─ 第 0 部分 信息区        P 图标 + 「PocketWeb / PocketWeb / 作者:shgaol」
   │      ├─ 第 1 部分 按钮列        QScrollArea（可滚动）＝ 已附加到侧边栏的小程序按钮
   │      └─ 展开/收起按钮           固定在底部（展开时显示文字「收起展开」）
   │
   └─ QTabWidget                    右侧页签区（占满剩余宽度）
      ├─ 页签 1「网页小程序」  CDSWebAppletPage   小程序表页（常驻，不可关闭）
      └─ 页签 2「网页」        CDSMdiArea          QMdiArea（TabbedView）
                                 └─ MDI 标签 × N  每个标签 = 一个 CDSWebViewWindow
```

### 3.1 左侧导航栏

| 部分 | 内容 | 说明 |
|---|---|---|
| 第 0 部分 | 应用图标 + 三行文本 | 由 `main.cpp` 调用 `SetInfoIcon/SetInfoCode/SetInfoName/SetInfoText` 设置，**始终贴在顶端** |
| 第 1 部分 | 小程序按钮列 | `QScrollArea`，**占满第 0 部分以下的全部空间**；按钮不足时靠上紧凑排列（间距 6px），过多时自动出现竖向滚动条 |
| 固定按钮 | 展开 / 收起 | 始终在底部；展开态显示图标 + 文字「收起展开」，收起态窄条只显示图标 |

- **宽度**：展开 = 主窗口宽度的 **1/6**；收起 = **72px**（`CDSPocketWebWindow::updateNavBarWidth()`）。
- 按钮：展开态高 40px、水平方向可拉伸；收起态 40×40 正方形。
- 没有按钮时第 1 部分整列隐藏，侧边栏只剩信息区与展开/收起按钮。
- 滚动条样式：竖向 8px 宽、滑块蓝色 `#3B82F6`。收起态下 `72 − 8(滚动条) − 20(内边距) = 44px`，仍容得下 40px 宽的按钮。

> 原 CUINavBarItem 的「第 2 部分树形菜单」「第 3 部分底部按钮列」已被**整块删除**（含公开接口与信号）。

### 3.2 右侧页签区

两个固定页签（不可关闭）：

- **页签 1「网页小程序」**：`CDSWebAppletPage` —— 小程序表页，启动时就常驻创建。
- **页签 2「网页」**：`CDSMdiArea` —— 网页窗口在这里以 MDI 标签形式打开。

程序启动默认停在页签 1（`main.cpp` 调 `openWebApplets()` 确认一次）。

### 3.3 网页窗口（MDI 标签）

每个标签是一个 `CDSWebViewWindow`（继承 `QMainWindow`）：工具栏 = `←` `→` `刷新` + **地址栏** + 「登录数据」按钮（外部站点窗口才有），中央是 `CDSWebEngineView`。

- 子窗口以 `showMaximized()` 打开；`QMdiArea` 设了 `DontMaximizeSubWindowOnActivation`，避免每次切标签都做一轮 `showNormal/showMaximized`（网页视图最怕这种反复折腾）。
- 标签图标：先放「已存图标 / 名称首字兜底」，网页自己的 favicon 解出来后替换。

### 3.4 拖出的独立窗口

把 MDI 标签往外拖（超过 `QApplication::startDragDistance()`）→ 该网页脱离 MDI，成为独立顶级窗口（`CDSDetachedWindow`）。拖回「网页」区域并松手 → 重新停成 MDI 标签、激活并最大化。

### 3.5 系统托盘

关闭窗口或最小化 → 隐藏到右下角通知区（不退出）；托盘图标右键菜单「显示」/「退出」；双击托盘图标 = 显示。

---

## 4. 功能清单

### 4.1 网页小程序管理（页签 1）

- **增加**：弹 `CDSWebAppletDlg`，名称必填且不能重复（忽略大小写），网址必填并自动补 `https://`（`www.baidu.com` → `https://www.baidu.com`）。
- **修改**：同一对话框；改网址会重新抓图标，改名称/网址会触发侧边栏同步。
- **删除**：二次确认后删除条目、连同图标 PNG；**侧边栏里对应的按钮一并移除**。
- **双击**（或选中后回车）：在页签 2 的 MDI 中打开该网页。
- 表页常驻，数据来自 `webapplets.json`，重新打开程序不会丢。

### 4.2 图标

三种来源，逐级保障：

1. **保存后立即异步抓取**（`CDSWebAppletIconFetcher`）：用隐藏的 `QWebEnginePage` 加载该网址，按 Chromium 的顺序取值 —— ① `iconChanged`（已解码好的图标）② `iconUrlChanged` 给出的图标地址下载 ③ `<站点>/favicon.ico` 兜底。
2. **启动时补齐**：对「图标文件不存在」的条目自动重抓一次，不必手工点「修改 → 确定」。
3. **打开网页后补存**：网页窗口解出 favicon 时会**顺手存下来**（**只在还没有图标文件时写**，不覆盖已抓到的图标）。

因为第 3 条与 MDI 标签图标同源（都是 Chromium 解出的 favicon），所以 **侧边栏按钮 / 表页条目 / MDI 标签三处图标一致**。

图标统一存成 **64×64 PNG**，文件名用 UUID（避免名称里的非法字符落到文件名上）。

### 4.3 打开网页窗口

- 在 MDI 中以 `小程序: <名称>` 为键去重，重复双击只激活已打开的窗口（无论它当前是 MDI 标签还是被拖出的独立窗口）。
- **profile（登录信息 + 缓存）按网址复用**：网址属于内置站点预设（`deepseek.com` / `toutiao.com` / `github.com`）就用该预设的 profile，与之前侧边栏按钮打开的窗口完全同一份数据，**不用重新登录**；其它网址用网页小程序自己的 profile。
- **站内链接**（由小程序网址推出的域名后缀，例如 `chat.deepseek.com` → `deepseek.com`）在窗口内导航；**站外链接**交给外部浏览器（Edge）。
- 窗口内「登录数据」按钮可直接打开该站点的数据目录（便于备份/迁移登录态）。

### 4.4 附加到侧边栏 / 移除

- 表页里右键某个小程序 → **「附加到侧边栏」**（已附加的置灰）。
- 侧边栏里右键那个小程序按钮 → **「移除」**。
- 点击侧边栏按钮 = 打开该小程序的网页（与双击表页条目等价）。
- 清单存在 `sidebar_applets.json`（**只存名称**，按附加顺序）。
- **小程序被删除或改名** → 名称匹配不上 → 下次同步时自动从清单和侧边栏里剔除。

### 4.5 标签拖出 / 拖回停靠

| 动作 | 结果 |
|---|---|
| 按住「网页」页签里的某个标签往外拖 | 该网页脱离 MDI，变成独立窗口，出现在光标处；并立即把移动交给系统（`startSystemMove`），拖动无缝衔接 |
| 拖动独立窗口，光标进入「网页」区域 | 该区域显示蓝色高亮边框（提示可放下） |
| 松手（光标在区域内，且此前离开过该区域） | 重新停成 MDI 标签，**自动激活该标签并最大化** |
| 松手（不在区域内） | 保持独立窗口 |
| 关闭独立窗口 | 关掉该网页（与点 MDI 标签的 × 一致） |

> 「必须先离开过一次」是刻意的：新窗口生成在光标处，那一刻光标本来就在区域内，不加这个条件会导致「刚拖出、手一松就被吸回去」。

### 4.6 网页右键菜单

无论右键落在**链接、按钮还是空白处**，菜单最前面都有一项（后面跟一条分隔线）：

- 点在链接上 → **「使用默认浏览器打开链接」**
- 其它位置 → **「使用默认浏览器打开此页面」**

走 `QDesktopServices::openUrl()`，即系统默认浏览器。

### 4.7 多实例

**程序不限制实例数量**，可以同时打开多个，各窗口彼此独立。

> 原「再次启动即激活已有实例」的命名管道单实例机制已按要求取消，`showWindow()` 现在只由托盘菜单和托盘双击调用。

---

## 5. 数据文件与目录

全部在 **`文档/PocketWeb/configure/`** 下（由 `CApplication::configDir()` 提供，与 DSH-Environment 完全隔离）：

```
文档/PocketWeb/configure/
├─ webapplets/
│  ├─ webapplets.json            小程序清单 {"applets":[{"name","url","icon"}]}
│  └─ icons/<uuid>.png           抓到的网页图标（64×64 PNG）
├─ sidebar_applets.json          侧边栏清单 {"applets":["名称1","名称2"]}
├─ env.json                      环境设置（Git/Node 目录、镜像地址）
├─ deepseek-web/                 DeepSeek 站点 profile
├─ toutiao-web/                  今日头条站点 profile
├─ github-shgaol-web/            GitHub 站点 profile
└─ webapplet-web/                其它网址共用的 profile
   └─ （每个站点目录下）storage/（cookie、localStorage、IndexedDB…）+ cache/（HTTP 缓存）
```

这些 profile 目录与 Edge 等系统浏览器**完全隔离**，清理浏览器缓存/Cookie 不会影响这里的登录状态；备份该目录即可迁移登录信息。

---

## 6. 源码结构与职责

| 文件 | 类 | 职责 |
|---|---|---|
| `main.cpp` | — | WebEngine 前置设置、CApplication、导航栏装配、启动页签 |
| `Application.h/.cpp` | `CApplication` | 全局配置：`configDir()`＝`文档/PocketWeb/configure`、env.json、服务端口管理 |
| `DSPocketWebWindow.h/.cpp` | `CDSPocketWebWindow` | 主窗口：导航栏 + 页签容器 + MDI；侧边栏小程序按钮的增删；打开网页窗口；托盘 |
| `NavBar/UINavBar.h/.cpp` | `CUINavBar` | 导航工作区容器 |
| `NavBar/UINavBarItem.h/.cpp` | `CUINavBarItem` | 导航栏实现：信息区 + 按钮列 + 展开/收起；`AddTopBtn/SetTopBtnIcon/RemoveTopBtn` |
| `WebApplet/DSWebAppletPage.h/.cpp` | `CDSWebAppletPage` | 小程序表页：列表、增改删、右键菜单、图标刷新 |
| `WebApplet/DSWebAppletDlg.h/.cpp` | `CDSWebAppletDlg` | 录入对话框（名称唯一性 + 网址规范化） |
| `WebApplet/DSWebAppletStore.h/.cpp` | `CDSWebAppletStore` | 小程序清单与图标文件存取 |
| `WebApplet/DSWebAppletIconFetcher.h/.cpp` | `CDSWebAppletIconFetcher` | 图标抓取（隐藏页面探测 + 下载 + favicon 兜底） |
| `DSMdiArea.h/.cpp` | `CDSMdiArea` | MDI 区域：空状态隐藏内部标签栏、识别标签拖出、停靠高亮、`dockBack()` |
| `DSDetachedWindow.h/.cpp` | `CDSDetachedWindow` | 拖出的独立网页窗口，判断何时该停靠回去 |
| `DSWebViewWindow.h/.cpp` | `CDSWebViewWindow` | 网页窗口本体：profile 选择、地址栏、站内判定、favicon 转发 |
| `DSWebEnginePage.h/.cpp` | `CDSWebEnginePage` | 站外链接交给 Edge；新窗口请求就地导航或外开 |
| `DSWebEngineView.h/.cpp` | `CDSWebEngineView` | 右键菜单追加「用默认浏览器打开…」 |
| `DSSidebarStore.h/.cpp` | `CDSSidebarStore` | 侧边栏清单存取 |
| `cmake/make_app_icon.ps1` | — | CMake 配置阶段用 PowerShell 生成 `resources/app.ico`（蓝底白 P，与 `makeLetterIcon` 同一套比例） |
| `cmake/organize_resource.cmake` | — | 归整 `Resource/` 布局并生成 `qt.conf` |

---

## 7. 关键常量与样式

| 项 | 值 | 位置 |
|---|---|---|
| 侧边栏展开宽度 | 主窗口宽度 ÷ 6 | `updateNavBarWidth()` |
| 侧边栏收起宽度 | 72px | `CUINavBarItem::applyExpandedState()` |
| 导航按钮 | 展开态高 40px / 收起态 40×40 | 同上 |
| 品牌蓝 | `#2563EB`（按钮选中、停靠高亮） | 多处 |
| 滚动条 | 竖向 8px，滑块 `#3B82F6` | `CUINavBarItem::initStyle()` |
| 抓取图标落盘尺寸 | 64×64 PNG | `DSWebAppletIconFetcher::kIconSize` |
| 图标探测超时 / 下载超时 | 20s / 15s | 同上 |
| 表页图标显示 / 格子 | 48×48 / 112×100 | `DSWebAppletPage.cpp` |
| MDI 标签高度 | 26px（QSS 压缩） | `CDSPocketWebWindow` 构造 |

---

## 8. 已知限制

1. **视频放不出来（H.264 / AAC）**
   本机这套 Qt 的 WebEngine 编译时**没有开启专有编解码器**：
   `qtwebenginecore-config_p.h` → `#define QT_FEATURE_webengine_proprietary_codecs -1`
   所以 B 站、优酷、腾讯视频、mp4 直链等 H.264 视频无法播放；VP8/VP9/AV1（YouTube 等）可以。
   **规避**：在页面上右键 →「使用默认浏览器打开此页面」。
   **根治**：用 `-webengine-proprietary-codecs` 重新编译 Qt WebEngine。

2. **个别站点抓不到图标要靠"打开一次"**
   有些站点（例如 `chat.deepseek.com`）的服务器对 `/favicon.ico` 不返回真正的图标文件（SPA 会把未知路径回落到 index.html），离线抓取必然失败。此时打开一次网页，窗口会把 Chromium 解出的 favicon 补存下来（见 4.2 第 3 条）。

3. **多实例会共用同一份数据**
   多个实例同时运行时会共用 `文档/PocketWeb/configure/`：
   - WebEngine 的 profile 目录（storage/cache）被多个进程同时使用，Chromium **不支持**这种用法，可能出现缓存/登录态互相干扰；
   - `webapplets.json` / `sidebar_applets.json` 是「整份读-改-写」，并发修改会**后写覆盖先写**；
   - 实例之间**没有跨进程通知**，A 实例的改动 B 实例不会自动刷新。

4. **侧边栏清单按名称关联**
   小程序改名后旧名称匹配不上，侧边栏项会被自动剔除，需要重新附加一次。

5. **定时器**
   全工程只有一处 `QTimer`：`CDSWebAppletIconFetcher` 的 20s 探测硬性截止（其余需要"延后执行"的地方一律用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`）。

---

## 9. 与 DSH-Environment 的关系

PocketWeb 是把 `E:\Trae-Project\DSH-Environment` 里的「网页小程序」功能独立出来的工程：

- **零改动搬运**：`WebApplet/*`、`DSWebViewWindow`、`DSWebEnginePage|View`、`NavBar/*`、`cmake/organize_resource.cmake` 基本逐字照搬；小程序的数据结构与读写规则保持一致。
- **唯一的功能性差异**：`Application::configDir()` 指向 `文档/PocketWeb/configure`（而不是 `文档/DSH-Environment/configure`），从而数据与上游完全隔离。
- **宿主结构差异**：上游把小程序表页也放进 MDI；本工程改成 QTabWidget 的第 1 个固定页签。
- **上游没有的部分**：侧边栏小程序按钮、标签拖出/拖回停靠、图标实时补存。
- **已裁剪掉的部分**：DSH 对话窗口、CMD 终端窗口、DSH 源码管理、导航栏的树形菜单与底部按钮列、单实例机制。

---

- # 联系方式
- shgaol@126.com
- ## ❤️ 支持本项目

如果您觉得这个项目对您有帮助，欢迎请我喝杯咖啡：

| 微信收款二维码 | 支付宝收款二为码 |
| --- | --- |
| <img width="435" height="444" alt="image" src="https://github.com/user-attachments/assets/d2808081-acd2-4df8-9ee8-620491a25667" /> | <img width="358" height="370" alt="image" src="https://github.com/user-attachments/assets/8a391c15-69a1-4bd2-9290-908a4a7535ec" /> |

