cmake_minimum_required(VERSION 3.16)

project(PocketWeb VERSION 1.0.0 LANGUAGES CXX)

# 调用已编译好的 Qt6 目录（Qt 源码目录为 Z:/QTSource/6.11.2/Src，仅供查阅）
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "Z:/QTSource/6.11.2/msvc2022_64")
endif()

# 生成的 exe 输出到「工作目录/<项目名称>-bin」（本工程 = PocketWeb-bin）。
# 目录名用 ${PROJECT_NAME} 拼出来，工程改名时目录名自动跟着变。
set(POCKETWEB_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/${PROJECT_NAME}-bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${POCKETWEB_OUTPUT_DIR}")
# 多配置生成器（Visual Studio / Ninja Multi-Config）默认会在上面这个目录后面再追加
# /<CONFIG>（于是 exe 落到 <项目名称>-bin/Release/，与部署脚本假定的目录不一致）。
# 这里逐配置覆盖一遍，保证无论用哪种生成器，exe 都直接落在 <项目名称>-bin 根目录。
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${POCKETWEB_OUTPUT_DIR}")
endforeach()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 Qt 的自动处理（MOC / RCC；本工程不使用 .ui 文件，故不开 AUTOUIC）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 同时兼容 Qt6 与 Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# 网页显示后端探测（按优先级）：
#  1) Qt WebEngine（唯一考虑的方向，不使用 QML 相关内容）
#  2) Qt WebView（仅 Qt6，Windows 上基于 Edge WebView2）
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS WebEngineWidgets)
if (NOT Qt${QT_VERSION_MAJOR}WebEngineWidgets_FOUND AND QT_VERSION_MAJOR GREATER_EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS WebView)
endif()

# 源码为 UTF-8：MSVC 需显式指定 /utf-8，否则中文注释/字符串会编译警告或乱码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================
# 应用图标 resources/app.ico（蓝色圆角方块 + 白色字母 P）
#   用途一：由 resources/app.rc 嵌入 exe → 资源管理器中显示的 exe 图标；
#   用途二：由 resources/resources.qrc 暴露成 :/app.ico → 程序窗口图标
#           与任务栏图标（main.cpp 里的 QIcon(":/app.ico")）。
#
#   .ico 是二进制文件，源码树里不便维护，所以这里在 CMake 配置阶段用 PowerShell
#   现场生成（脚本：cmake/make_app_icon.ps1）。画法与 NavBar/UINavBarItem.cpp 的
#   CUINavBarItem::makeLetterIcon() 完全一致：
#     0.04 边距 / 0.92 边长 / 0.22 圆角 / 白色加粗 0.58 字号 / 底色 #2563EB。
#   文件已存在则不再生成 —— 想换图标就删掉 resources/app.ico 后重新配置，
#   或者直接用自己的 .ico 覆盖它。
# ============================================================
set(_app_icon "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
if(NOT EXISTS "${_app_icon}")
    find_program(_powershell_exe NAMES powershell pwsh)
    if(_powershell_exe)
        message(STATUS "生成应用图标: ${_app_icon}")
        execute_process(
            COMMAND "${_powershell_exe}" -NoProfile -NonInteractive
                    -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/cmake/make_app_icon.ps1"
                    -OutPath "${_app_icon}"
            RESULT_VARIABLE _icon_result)
        if(NOT _icon_result EQUAL 0)
            message(WARNING "make_app_icon.ps1 执行失败（返回码 ${_icon_result}）")
        endif()
    else()
        message(WARNING "未找到 powershell/pwsh，无法生成 resources/app.ico")
    endif()
endif()
if(NOT EXISTS "${_app_icon}")
    message(FATAL_ERROR
        "缺少 ${_app_icon}，且自动生成失败。\n"
        "请手工放置一个 .ico 到该路径后重新配置 CMake。该图标同时用于：\n"
        "  1) 嵌入 exe（资源管理器里显示的程序图标）；\n"
        "  2) 侧边栏 InfoIcon 与窗口图标（:/app.ico）。\n"
        "否则 rc.exe / rcc 会因找不到文件而构建失败。")
endif()

add_executable(PocketWeb WIN32
    main.cpp
    Application.h
    Application.cpp
    DSPocketWebWindow.h
    DSPocketWebWindow.cpp
    DSWebViewWindow.h
    DSWebViewWindow.cpp
    DSWebEnginePage.h
    DSWebEnginePage.cpp
    DSWebEngineView.h
    DSWebEngineView.cpp
    DSMdiArea.h
    DSMdiArea.cpp
    DSDetachedWindow.h
    DSDetachedWindow.cpp
    DSSidebarStore.h
    DSSidebarStore.cpp
    WebApplet/DSWebAppletStore.h
    WebApplet/DSWebAppletStore.cpp
    WebApplet/DSWebAppletDlg.h
    WebApplet/DSWebAppletDlg.cpp
    WebApplet/DSWebAppletIconFetcher.h
    WebApplet/DSWebAppletIconFetcher.cpp
    WebApplet/DSWebAppletPage.h
    WebApplet/DSWebAppletPage.cpp
    NavBar/UINavBarItem.h      # CUINavBarItem：导航栏（第 0 部分信息区 / 第 1 部分按钮）
    NavBar/UINavBarItem.cpp
    NavBar/UINavBar.h          # CUINavBar：导航工作区容器
    NavBar/UINavBar.cpp
    resources/resources.qrc    # 应用图标资源（:/app.ico → 窗口图标）
    resources/app.rc           # 把 app.ico 嵌入 exe（资源管理器显示的程序图标）
)

target_link_libraries(PocketWeb PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# MinGW：显式静态链接 GCC 运行时（libgcc / libstdc++ / libwinpthread），
# 避免运行时报 XXX.dll 找不到（Qt 静态库场景下必需）
if(MINGW)
    target_link_options(PocketWeb PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# 网页显示后端：优先 WebEngine，其次 Qt6 WebView，都没有则占位
if (Qt${QT_VERSION_MAJOR}WebEngineWidgets_FOUND)
    target_compile_definitions(PocketWeb PRIVATE DSH_HAVE_WEBENGINE)
    target_link_libraries(PocketWeb PRIVATE Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
elseif (Qt${QT_VERSION_MAJOR}WebView_FOUND AND QT_VERSION_MAJOR GREATER_EQUAL 6)
    target_compile_definitions(PocketWeb PRIVATE DSH_HAVE_WEBVIEW)
    target_link_libraries(PocketWeb PRIVATE Qt${QT_VERSION_MAJOR}::WebView)
endif()

# ============================================================
# 部署 Qt 运行库（仅 Qt6，动态库场景）
#   下面用 <OUT> 代指 ${POCKETWEB_OUTPUT_DIR}，即「工作目录/<项目名称>-bin」
#   （本工程 = PocketWeb-bin）：
#     <OUT>/PocketWeb.exe            生成的 exe（见上方 RUNTIME_OUTPUT_DIRECTORY）
#     <OUT>/Qt6*.dll 等               直接调用的 Qt dll（与 exe 同目录，由系统加载）
#     <OUT>/QtWebEngineProcess.exe   WebEngine 辅助进程（必须与 Qt6WebEngineCore.dll
#                                    同目录，否则加载失败导致程序退出，不能移入 Resource）
#     <OUT>/Resource/qml/            QML 模块（Qt WebEngine 运行所需，非本工程源码）
#     <OUT>/Resource/resources/      WebEngine 数据（icudtl.dat、*.pak 等）
#     <OUT>/Resource/translations/   Qt 翻译 / WebEngine 语言包
#     <OUT>/Resource/plugins/        Qt 插件
#     <OUT>/qt.conf                  路径配置（由 organize_resource.cmake 生成）
#   说明：只拷贝程序正常运行需要的文件（windeployqt 按依赖分析拷贝）；
#         qml/resources/translations/plugins 全部位于 Resource，
#         启动时由 setupResourceDirByWinApi() 与 qt.conf 指向这些目录。
# ============================================================
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    # ---- ① Qt 运行库（dll）→ <项目名称>-bin 根目录 ----
    # windeployqt 按依赖分析把本程序直接调用的 Qt dll（Qt6Core/Gui/Widgets/Network/
    # WebEngineCore/WebEngineWidgets…）以及 QtWebEngineProcess.exe 拷到 exe 同目录，
    # 即 <项目名称>-bin/。插件/翻译用 --plugindir/--translationdir 直接落到它的 Resource 下。
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    if(_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
        set(_windeployqt "${_qt_bin_dir}/windeployqt.exe")

        add_custom_command(TARGET PocketWeb POST_BUILD
            COMMAND "${_windeployqt}"
                    --no-system-d3d-compiler   # Win10 系统自带 d3dcompiler_47
                    --no-opengl-sw             # 不需要软件 OpenGL 渲染器
                    --plugindir "$<TARGET_FILE_DIR:PocketWeb>/Resource/plugins"
                    --translationdir "$<TARGET_FILE_DIR:PocketWeb>/Resource/translations"
                    "$<TARGET_FILE:PocketWeb>"
            COMMENT "windeployqt: Qt dll → <项目名称>-bin，plugins/translations → Resource"
        )
    else()
        message(WARNING "未找到 Qt6::qmake，跳过 windeployqt：输出目录下不会自动出现 Qt dll/插件")
    endif()

    # ---- ② 其余运行时文件归整到 Resource/ + 生成 qt.conf ----
    # 与 windeployqt 分开注册，保证「即使 windeployqt 没找到/没跑」也一定会生成
    # <项目名称>-bin/qt.conf（否则 Qt 找不到 plugins/translations/resources，程序起不来）。
    # 脚本幂等：只搬运确实存在于该目录根下的文件，qt.conf 每次覆盖重写。
    #   - WebEngine 数据（icudtl.dat、*.pak、v8_context_snapshot.bin…）→ Resource/resources
    #   - QML 模块（Qt6 的 windeployqt 会把 qml 部署到 exe 同目录）→ Resource/qml
    #   - translations 残留 → Resource/translations
    #   - qt.conf：Plugins/QmlImports/Translations/Data 全部指到 Resource 下
    # 注意：QtWebEngineProcess.exe 必须留在 <项目名称>-bin 根目录（与 Qt6WebEngineCore.dll
    #       同目录），移进 Resource 会导致它起不来、WebEngine 初始化失败、程序直接退出。
    add_custom_command(TARGET PocketWeb POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DBIN_DIR="$<TARGET_FILE_DIR:PocketWeb>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/organize_resource.cmake"
        COMMENT "归整 Resource 布局并生成 qt.conf"
    )
endif()
