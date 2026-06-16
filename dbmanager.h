#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QString>

// ============================================================
// DbManager — 数据库连接与 schema 管理 Singleton
//
// 职责：
//   1. 数据库自举：先尝试无库连接并 CREATE DATABASE IF NOT EXISTS
//   2. 建立 QMARIADB 连接（仅 main 线程，无连接池）
//   3. 执行 schema evolution：CREATE TABLE IF NOT EXISTS +
//      ALTER TABLE ADD COLUMN IF NOT EXISTS（通过忽略 1060
//      错误码实现幂等性）
//
// 线程安全：
//   非线程安全。Qt SQL 模块要求所有数据库操作在同一线程，
//   本应用所有数据库访问均在主（GUI）线程完成。
//
// 为什么没有连接池？
//   桌面应用只有一个用户，单一连接即可满足需求。连接池
//   在服务端场景中才需要引入（如为 chat_server 增加 MySQL
//   直连能力时）。
// ============================================================

class DbManager
{
public:
    static DbManager &instance();

    QSqlDatabase database() const;
    bool isConnected() const;

    bool initialize(const QString &host, int port,
                    const QString &user, const QString &password,
                    const QString &dbName);

private:
    DbManager();
    ~DbManager();
    DbManager(const DbManager &) = delete;
    DbManager &operator=(const DbManager &) = delete;

    bool createTables();
    bool ensureDatabaseExists(const QString &host, int port,
                              const QString &user, const QString &password,
                              const QString &dbName);

    QSqlDatabase m_db;
    bool m_connected;
};

#endif // DBMANAGER_H
