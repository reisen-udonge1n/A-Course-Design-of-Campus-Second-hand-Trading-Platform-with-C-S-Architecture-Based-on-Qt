// 引入验证码管理器的头文件
#include "verificationcodemanager.h"
// 引入 Qt 的随机数生成器，用于生成安全的随机验证码
#include <QRandomGenerator>
// 引入调试输出头文件，用于在开发阶段打印日志
#include <QDebug>

// 获取 VerificationCodeManager 单例实例的静态方法（Meyer's Singleton）
VerificationCodeManager &VerificationCodeManager::instance()
{
    // 定义局部 static 变量，C++11 起保证线程安全的初始化
    static VerificationCodeManager mgr;
    // 返回唯一实例的引用
    return mgr;
}

// 为指定邮箱生成一个 6 位数字验证码
// 参数 email：目标邮箱地址，用作验证码的索引键
// 返回值：生成的 6 位数字验证码字符串
QString VerificationCodeManager::generateCode(const QString &email)
{
    // 清理过期验证码：遍历当前所有验证码，移除超过 5 分钟未使用的
    QDateTime now = QDateTime::currentDateTime();  // 获取当前时间
    // 使用迭代器遍历存储验证码的哈希表
    for (auto it = m_codes.begin(); it != m_codes.end(); ) {
        // 计算该验证码的创建时间到现在的秒数
        // 如果已超过 300 秒（5 分钟），则视为过期并移除
        if (it->createdAt.secsTo(now) >= 300)
            it = m_codes.erase(it);  // erase 返回下一个有效迭代器
        else
            ++it;                     // 未过期则继续检查下一个
    }

    // 使用全局随机数生成器生成 100000 到 999999 之间的随机整数
    int num = QRandomGenerator::global()->bounded(100000, 999999);
    // 将整数转换为字符串形式，得到 6 位数字验证码
    QString code = QString::number(num);

    // 将新生成的验证码存入哈希表，键为邮箱地址
    // 值为 CodeInfo 结构体，包含验证码文本和创建时间
    m_codes[email] = {code, now};
    // 在调试控制台输出验证码生成信息，方便开发调试
    qDebug() << "验证码已生成:" << email << code;
    // 返回生成的验证码字符串
    return code;
}

// 验证指定邮箱的验证码是否正确
// 参数 email：邮箱地址
// 参数 code：用户输入的验证码
// 返回值：true 表示验证通过，false 表示验证失败
bool VerificationCodeManager::verifyCode(const QString &email, const QString &code)
{
    // 在哈希表中查找该邮箱对应的验证码记录
    auto it = m_codes.find(email);
    // 如果找不到记录，说明从未生成过验证码或已被移除
    if (it == m_codes.end())
        return false;  // 验证失败

    // 检查 5 分钟有效期：计算验证码创建时间到当前时间的秒数
    if (it->createdAt.secsTo(QDateTime::currentDateTime()) >= 300) {
        // 验证码已过期，从哈希表中移除该记录
        m_codes.erase(it);
        return false;  // 验证失败
    }

    // 比对用户输入的验证码和存储的验证码是否一致
    if (it->code != code)
        return false;  // 验证码不匹配，验证失败

    // 验证码正确且在有效期内，验证成功
    // 清除验证码防止同一验证码被重复使用（防重放攻击）
    m_codes.erase(it);
    return true;  // 验证通过
}

// 手动移除指定邮箱的验证码
// 参数 email：要移除验证码的邮箱地址
// 在用户重新请求验证码时，需要先清除旧的验证码
void VerificationCodeManager::removeCode(const QString &email)
{
    // 从哈希表中移除该邮箱对应的验证码记录
    // 如果该邮箱不存在记录，remove 操作不做任何事
    m_codes.remove(email);
}
