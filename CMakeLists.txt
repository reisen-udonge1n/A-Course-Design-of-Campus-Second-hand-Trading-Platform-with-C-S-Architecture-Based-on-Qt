# ============================================================
# CMake 构建配置 — 双目标项目
#
# 目标 1: secondhand_trading（GUI 应用）
#   链接 Qt Widgets + Sql + Network
#   包含所有 UI 页面、数据库管理器、聊天客户端、SMTP、
#   Filter、资源文件等
#
# 目标 2: chat_server（无头服务器）
#   仅链接 Qt Network
#   独立的 TCP+HTTP 双协议服务器
#
# 构建系统特性：
#   - AUTOMOC: 自动为 Q_OBJECT 类生成 MOC 文件
#   - AUTOUIC: 自动从 .ui 文件生成 UI 头文件
#   - AUTORCC: 自动编译 .qrc 资源文件
#   - Qt6 优先，找不到则回退 Qt5 5.15+
#
# MinGW 特别注意：
#   qsqlmysql.dll 依赖 libmysql.dll、libssl、libcrypto，
#   windeployqt 不会处理这些——因此在 build 后通过自定义
#   POST_BUILD 命令将这些 DLL 复制到输出目录。
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(secondhand_trading LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# 指定 UI 文件搜索路径
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

# 查找 Qt（优先 Qt6，回退 Qt5）
find_package(Qt6 COMPONENTS Widgets Sql Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Sql Network)
endif()

# 可执行目标
add_executable(${PROJECT_NAME}
    main.cpp
    logwindow.h
    logwindow.cpp
    registerdialog.h
    registerdialog.cpp
    mainwindow.h
    mainwindow.cpp
    homepage.h
    homepage.cpp
    forgotpassworddialog.h
    forgotpassworddialog.cpp
    profilepage.h
    profilepage.cpp
    editprofiledialog.h
    editprofiledialog.cpp
    orderlistpage.h
    orderlistpage.cpp
    smtpsender.h
    smtpsender.cpp
    verificationcodemanager.h
    verificationcodemanager.cpp
    publishpage.h
    publishpage.cpp
    productspage.h
    productspage.cpp
    productdetaildialog.h
    productdetaildialog.cpp
    chatlistpage.h
    chatlistpage.cpp
    chatclient.h
    chatclient.cpp
    chatdialog.h
    chatdialog.cpp
    chatbubble.h
    chatbubble.cpp
    dbmanager.h
    dbmanager.cpp
    searchpage.h
    searchpage.cpp
    util.h
    filter.h
    filter.cpp
    followersdialog.cpp
    followersdialog.h
    userproductsdialog.h
    userproductsdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/registerdialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/forgotpassworddialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/editprofiledialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/notificationpopup.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/homepage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/productspage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/searchpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/profilepage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/orderlistpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/publishpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/chatlistpage.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/chatdialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/productdetaildialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/userproductsdialog.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/followersdialog.ui
    image.qrc
)

# 链接 Qt 库
if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Sql Qt5::Network)
endif()

# 输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# 聊天服务器（独立可执行文件）
add_executable(chat_server
    server_main.cpp
    chatserver.h
    chatserver.cpp

)

if(Qt6_FOUND)
    target_link_libraries(chat_server PRIVATE Qt6::Network)
else()
    target_link_libraries(chat_server PRIVATE Qt5::Network)
endif()

set_target_properties(chat_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# ============================================================
# Auto-deploy MySQL runtime DLLs to output directory.
# qsqlmysql.dll depends on libmysql.dll, which depends on
# libssl and libcrypto from the MinGW toolchain. These are
# NOT handled by windeployqt, so we copy them manually.
#
# If any of these DLLs are missing at build time, a warning
# is printed — the application may still work if the DLLs
# are already in the system PATH.
# ============================================================
if(Qt6_FOUND)
    get_filename_component(_qt6_cmake_dir "${Qt6_DIR}" DIRECTORY)
    get_filename_component(_qt6_lib_dir "${_qt6_cmake_dir}" DIRECTORY)
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_bin_dir "${_qt6_prefix}/bin")
endif()

find_file(LIBMYSQL_DLL libmysql.dll
    HINTS ${_qt6_bin_dir}
    DOC "MySQL client library needed at runtime by qsqlmysql.dll"
)

get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_file(LIBSSL_DLL libssl-3-x64.dll
    HINTS ${_compiler_bin}
    DOC "OpenSSL library needed by libmysql.dll"
)
find_file(LIBCRYPTO_DLL libcrypto-3-x64.dll
    HINTS ${_compiler_bin}
    DOC "OpenSSL crypto library needed by libmysql.dll"
)

foreach(_dll_var LIBMYSQL_DLL LIBSSL_DLL LIBCRYPTO_DLL)
    if(${_dll_var})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${${_dll_var}}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Deploying: ${${_dll_var}}"
        )
    else()
        message(WARNING "${_dll_var} not found — MySQL connection may fail at runtime")
    endif()
endforeach()
