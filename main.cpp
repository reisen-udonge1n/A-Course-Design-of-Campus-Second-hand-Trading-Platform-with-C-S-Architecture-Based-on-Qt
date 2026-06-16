#include <QApplication>
#include <QMessageBox>
#include "config.h"
#include "logwindow.h"
#include "dbmanager.h"

// ============================================================
// 应用入口：Singleton 服务初始化 → LoginWindow → MainWindow
//
// 架构说明：
//   这是一个「单页应用」风格的 Qt Desktop 应用——所有主界面
//   都在 MainWindow 的 QStackedWidget 中切换，没有多窗口。
//   DbManager 在启动时完成数据库自举（CREATE DATABASE IF NOT
//   EXISTS + schema evolution），确保零配置即可运行。
//
// 两个独立进程：
//   - secondhand_trading.exe（本文件）：用户界面 + 数据库直连
//   - chat_server.exe：独立部署的 TCP+HTTP 双协议服务器
//     （消息路由 + 图片存储服务）
// ============================================================

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    
    // 设置应用图标
    app.setWindowIcon(QIcon(":/new/prefix1/image/icon.png"));

    // 【启动时数据库自举】
    // DbManager 是线程不安全的 Singleton，生命周期与进程相同。
    // initialize() 内部会先尝试无库连接以执行 CREATE DATABASE，
    // 再以目标库名重连并执行 schema evolution。
    if (!DbManager::instance().initialize(DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_NAME)) {
        QMessageBox::critical(nullptr, "数据库错误",
                              "无法连接到 MySQL 数据库，请检查网络连接和数据库配置。");
        return 1;
    }

    // LoginWindow 验证成功后发射 loginSuccess() 信号，
    // 由 logwindow.cpp 内部的连接创建 MainWindow 并销毁自身。
    LoginWindow login;
    login.show();

    return app.exec();
}
