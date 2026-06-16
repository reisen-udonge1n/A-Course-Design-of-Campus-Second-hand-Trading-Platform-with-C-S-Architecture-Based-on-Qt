// 引入数据库管理器的头文件，其中声明了 DbManager 类
#include "dbmanager.h"
// 引入 SQL 查询类，用于执行 SQL 语句
#include <QSqlQuery>
// 引入 SQL 错误类，用于获取数据库操作的错误信息
#include <QSqlError>
// 引入调试输出头文件，用于打印数据库操作日志
#include <QDebug>

// 【Meyer's Singleton】
// C++11 起局部 static 变量初始化是线程安全的，但 DbManager
// 内部状态仍需调用方保证单线程访问。
// 获取 DbManager 单例实例的静态方法
DbManager &DbManager::instance()
{
    // 定义局部 static 变量，C++11 起保证线程安全的初始化
    // 程序结束时自动销毁
    static DbManager mgr;
    // 返回唯一实例的引用
    return mgr;
}

// DbManager 构造函数：初始化成员变量
DbManager::DbManager()
    // 初始化连接状态为 false，表示尚未建立数据库连接
    : m_connected(false)
{
}

// DbManager 析构函数：释放数据库连接资源
DbManager::~DbManager()
{
    // 如果数据库连接已打开，则关闭连接
    if (m_db.isOpen())
        m_db.close();
}

// 获取当前数据库连接对象（QSqlDatabase）的常量引用
QSqlDatabase DbManager::database() const
{
    // 返回成员变量中保存的数据库连接
    return m_db;
}

// 检查数据库是否已成功连接
// 返回值：true 表示已连接，false 表示未连接或连接失败
bool DbManager::isConnected() const
{
    // 返回连接状态标志
    return m_connected;
}

// 初始化数据库连接
// 参数 host：MySQL 服务器主机地址
// 参数 port：MySQL 服务器端口号
// 参数 user：数据库用户名
// 参数 password：数据库密码
// 参数 dbName：目标数据库名称
// 返回值：true 表示初始化成功，false 表示失败
bool DbManager::initialize(const QString &host, int port,
                            const QString &user, const QString &password,
                            const QString &dbName)
{
    // 第一步：确保数据库存在（如果不存在则自动创建）
    ensureDatabaseExists(host, port, user, password, dbName);

    // 创建正式的数据库连接，连接名称为 "main_connection"
    // 使用 QMARIADB 驱动（Qt 的 MariaDB/MySQL 驱动）
    m_db = QSqlDatabase::addDatabase("QMARIADB", "main_connection");
    // 设置数据库主机地址
    m_db.setHostName(host);
    // 设置数据库端口
    m_db.setPort(port);
    // 设置数据库用户名
    m_db.setUserName(user);
    // 设置数据库密码
    m_db.setPassword(password);
    // 设置目标数据库名称
    m_db.setDatabaseName(dbName);

    // 输出当前 Qt 可用的所有 SQL 驱动列表（用于调试和排查驱动问题）
    qDebug() << "可用 SQL 驱动:" << QSqlDatabase::drivers();
    // 尝试打开数据库连接
    if (!m_db.open()) {
        // 连接失败时输出详细的错误信息
        qDebug() << "数据库连接失败:" << m_db.lastError().text();
        return false;  // 初始化失败
    }

    // 标记数据库连接状态为已连接
    m_connected = true;

    // 创建所有必要的数据库表（如果尚不存在）
    if (!createTables()) {
        // 表创建失败时输出日志
        qDebug() << "数据库表初始化失败";
        return false;  // 初始化失败
    }

    // 输出数据库连接成功的日志，包含主机、端口和数据库名称
    qDebug() << "MySQL 数据库连接成功:" << host << port << dbName;
    return true;  // 初始化成功
}

// 【数据库自举：两阶段初始化】
// 第一阶段：使用临时连接（不指定数据库名）执行 CREATE DATABASE
// 第二阶段：以目标数据库名建立正式连接并执行 schema evolution
//
// 这种设计让应用"开箱即用"——只需 MySQL 服务器运行中且配置
// 正确，无需手动创建数据库。
// 确保数据库存在，如果不存在则自动创建
// 参数含义与 initialize 方法相同
// 返回值：true 表示数据库存在或创建成功，false 表示出错
bool DbManager::ensureDatabaseExists(const QString &host, int port,
                                      const QString &user, const QString &password,
                                      const QString &dbName)
{
    // 创建一个临时数据库连接用于自举，连接名称为 "temp_bootstrap"
    // 该连接不指定数据库名，直接连接到 MySQL 服务器
    QSqlDatabase tempDb = QSqlDatabase::addDatabase("QMARIADB", "temp_bootstrap");
    // 设置主机地址
    tempDb.setHostName(host);
    // 设置端口
    tempDb.setPort(port);
    // 设置用户名
    tempDb.setUserName(user);
    // 设置密码
    tempDb.setPassword(password);

    // 尝试打开临时连接
    if (!tempDb.open()) {
        // 连接 MySQL 服务器失败，输出错误信息
        qDebug() << "无法连接到 MySQL 服务器:" << tempDb.lastError().text();
        // 清理临时连接，避免连接名冲突
        QSqlDatabase::removeDatabase("temp_bootstrap");
        return false;
    }

    // 创建一个查询对象，关联到临时数据库连接
    QSqlQuery q(tempDb);
    // 执行 CREATE DATABASE IF NOT EXISTS 语句
    // 使用反引号包裹数据库名称以支持特殊字符
    // 设置字符集为 utf8mb4 以支持完整的 Unicode（包括 Emoji）
    bool ok = q.exec(QString("CREATE DATABASE IF NOT EXISTS `%1` CHARACTER SET utf8mb4").arg(dbName));
    // 如果创建失败，输出错误信息
    if (!ok)
        qDebug() << "创建数据库失败:" << q.lastError().text();

    // 关闭临时数据库连接
    tempDb.close();
    // 移除临时连接（释放资源），避免后续添加同名连接时出错
    QSqlDatabase::removeDatabase("temp_bootstrap");
    // 返回 CREATE DATABASE 的执行结果
    return ok;
}

// 【Schema Evolution 策略】
// 使用 CREATE TABLE IF NOT EXISTS + ALTER TABLE ADD COLUMN 的
// 组合模式。ALTER 通过忽略 MySQL 1060 错误（Duplicate column）
// 来实现幂等性——这是向后兼容的轻量方案。
//
// 为什么不用 migration 工具（如 Flyway）？
//   桌面应用没有统一的服务端部署流程，不需要版本化迁移。
//   这种"应用启动时自动调整 schema"的模式在这类场景中
//   更实用。
//
// 注意：MODIFY COLUMN 不是幂等的——每次启动都会执行。
// 这在此处是可接受的（将 images 列从 TEXT 升级为 LONGTEXT），
// 但在生产环境中应谨慎使用。
// 创建所有必要的数据库表，并对旧表执行 schema 升级
// 返回值：true 表示成功
bool DbManager::createTables()
{
    // 创建 SQL 查询对象，关联到主数据库连接
    QSqlQuery q(m_db);

    // 关闭外键检查以简化建表顺序
    // 这样可以在子表引用父表之前创建子表
    q.exec("SET FOREIGN_KEY_CHECKS = 0");

    // ----- 创建 users 表（用户账户）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT AUTO_INCREMENT PRIMARY KEY,   -- 用户 ID，自增主键
            username VARCHAR(255) NOT NULL UNIQUE, -- 用户名，唯一约束
            email VARCHAR(255) NOT NULL UNIQUE,    -- 邮箱地址，唯一约束
            password_hash VARCHAR(255) NOT NULL    -- SHA-256 密码哈希值
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4    -- InnoDB 引擎支持事务和外键
    )");

    // 添加 session_token 字段（兼容 MySQL 5.x，通过 DESCRIBE 检查字段是否存在）
    q.exec("DESCRIBE users session_token");
    if (!q.next()) {
        q.exec("ALTER TABLE users ADD COLUMN session_token VARCHAR(64) DEFAULT ''");
    }

    // ----- 创建 products 表（商品信息）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS products (
            id INT AUTO_INCREMENT PRIMARY KEY,   -- 商品 ID，自增主键
            title VARCHAR(255) NOT NULL,          -- 商品标题
            price DECIMAL(10,2) NOT NULL,         -- 商品价格（最高 10 位，小数 2 位）
            description TEXT,                     -- 商品描述
            image TEXT,                           -- 商品图片（旧字段，兼容老版本）
            seller_id INT NOT NULL,               -- 卖家用户 ID
            status INT DEFAULT 0,                 -- 商品状态：0=在售，1=已售
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 发布时间
            category VARCHAR(100) DEFAULT '',     -- 商品分类
            images TEXT DEFAULT '',               -- 多张图片（Base64 逗号分隔，LONGTEXT）
            FOREIGN KEY (seller_id) REFERENCES users(id)  -- 外键约束：引用 users 表
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )");

    // ----- 创建 orders 表（订单信息）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS orders (
            id INT AUTO_INCREMENT PRIMARY KEY,     -- 订单 ID，自增主键
            product_id INT NOT NULL,                -- 商品 ID
            buyer_id INT NOT NULL,                  -- 买家用户 ID
            seller_id INT NOT NULL,                 -- 卖家用户 ID
            status INT DEFAULT 0,                   -- 订单状态：0=待确认，2=待发货等
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 下单时间
            FOREIGN KEY (product_id) REFERENCES products(id),  -- 外键：引用商品
            FOREIGN KEY (buyer_id) REFERENCES users(id),       -- 外键：引用买家
            FOREIGN KEY (seller_id) REFERENCES users(id)       -- 外键：引用卖家
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )");

    // ----- 创建 messages 表（聊天消息）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INT AUTO_INCREMENT PRIMARY KEY,     -- 消息 ID，自增主键
            sender_id INT NOT NULL,                 -- 发送者用户 ID
            receiver_id INT NOT NULL,               -- 接收者用户 ID
            product_id INT DEFAULT 0,               -- 关联的商品 ID（0 表示不关联商品）
            content TEXT NOT NULL,                   -- 消息内容
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 发送时间
            is_read INT DEFAULT 0,                  -- 是否已读：0=未读，1=已读
            FOREIGN KEY (sender_id) REFERENCES users(id),    -- 外键：引用发送者
            FOREIGN KEY (receiver_id) REFERENCES users(id)   -- 外键：引用接收者
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )");

    // ----- 创建 user_profiles 表（用户扩展资料）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS user_profiles (
            user_id INT PRIMARY KEY,               -- 用户 ID，主键（与 users 表一对一）
            gender VARCHAR(10) DEFAULT '',          -- 性别
            address TEXT DEFAULT '',                -- 地址（通用）
            shipping_address TEXT DEFAULT '',       -- 发货地址
            receiving_address TEXT DEFAULT '',      -- 收货地址
            avatar TEXT DEFAULT '',                 -- 头像（Base64 编码）
            FOREIGN KEY (user_id) REFERENCES users(id)  -- 外键约束
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )");

    // ----- 创建 followers 表（关注关系）-----
    q.exec(R"(
        CREATE TABLE IF NOT EXISTS followers (
            id INT AUTO_INCREMENT PRIMARY KEY,      -- 记录 ID，自增主键
            follower_id INT NOT NULL,                -- 关注者用户 ID
            followed_id INT NOT NULL,                -- 被关注者用户 ID
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,  -- 关注时间
            FOREIGN KEY (follower_id) REFERENCES users(id),  -- 外键：引用关注者
            FOREIGN KEY (followed_id) REFERENCES users(id),  -- 外键：引用被关注者
            UNIQUE KEY unique_follow (follower_id, followed_id)  -- 联合唯一：防止重复关注
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )");

    // 建表完成后重新开启外键检查
    q.exec("SET FOREIGN_KEY_CHECKS = 1");

    // 对已存在的旧表补充可能缺失的列（忽略重复列错误）
    // 定义 lambda 辅助函数：尝试为指定表添加列
    // 如果列已存在，MySQL 会返回 1060 错误（Duplicate column name），忽略即可
    auto addColumn = [&](const QString &table, const QString &columnDef) {
        // 执行 ALTER TABLE ADD COLUMN 语句
        q.exec(QString("ALTER TABLE %1 ADD COLUMN %2").arg(table, columnDef));
        // MySQL 错误码 1060 = Duplicate column name，忽略即可
    };

    // ----- products 表的向后兼容列添加 -----
    addColumn("products", "status INT DEFAULT 0");              // 商品状态列
    addColumn("products", "category VARCHAR(100) DEFAULT ''");  // 商品分类列
    addColumn("products", "images TEXT DEFAULT ''");            // 多图片列
    addColumn("products", "created_at DATETIME DEFAULT CURRENT_TIMESTAMP");  // 发布时间列

    // 将 images 列升级为 LONGTEXT 以容纳 Base64 图片数据
    // 旧版本的 images 列是 TEXT 类型，最大 64KB，不足以存储 Base64 编码的图片
    // MODIFY COLUMN 不是幂等的——每次启动都会执行，但在开发阶段是可接受的
    q.exec("ALTER TABLE products MODIFY COLUMN images LONGTEXT");

    // ----- orders 表的向后兼容列添加 -----
    addColumn("orders", "status INT DEFAULT 0");               // 订单状态列
    addColumn("orders", "created_at DATETIME DEFAULT CURRENT_TIMESTAMP");  // 下单时间列
    addColumn("orders", "offer_price DECIMAL(10,2) DEFAULT NULL");  // 议价价格列（可为空）

    // ----- messages 表的向后兼容列添加 -----
    addColumn("messages", "product_id INT DEFAULT 0");         // 关联商品 ID 列
    addColumn("messages", "is_read INT DEFAULT 0");            // 已读状态列
    addColumn("messages", "created_at DATETIME DEFAULT CURRENT_TIMESTAMP");  // 发送时间列

    // ----- user_profiles 表的向后兼容列添加 -----
    addColumn("user_profiles", "gender VARCHAR(10) DEFAULT ''");            // 性别列
    addColumn("user_profiles", "address TEXT DEFAULT ''");                  // 地址列
    addColumn("user_profiles", "shipping_address TEXT DEFAULT ''");         // 发货地址列
    addColumn("user_profiles", "receiving_address TEXT DEFAULT ''");        // 收货地址列
    addColumn("user_profiles", "avatar TEXT DEFAULT ''");                   // 头像列

    // 所有表和列创建/升级成功
    return true;
}
