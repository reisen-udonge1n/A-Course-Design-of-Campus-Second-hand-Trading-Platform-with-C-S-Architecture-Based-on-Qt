#ifndef VERIFICATIONCODEMANAGER_H
#define VERIFICATIONCODEMANAGER_H

#include <QString>
#include <QMap>
#include <QDateTime>

// ============================================================
// VerificationCodeManager — 验证码管理 Singleton
//
// 纯内存实现，不涉及数据库。代码以 email 为键存储在 QMap 中。
//
// 策略：
//   - 6 位数字验证码（QRandomGenerator 生成）
//   - 5 分钟过期（generateCode 时清理过期条目）
//   - 同一邮箱重复调用 generateCode 会覆盖旧码
//
// 安全考量：
//   内存存储意味着进程重启后所有验证码失效，这实际上是
//   一个安全特性——防止旧验证码被重放。
//
// 改进方向：
//   生产环境应改用 Redis 或数据库存储，支持分布式部署
//   和更精确的过期策略。
// ============================================================

class VerificationCodeManager
{
public:
    static VerificationCodeManager &instance();

    QString generateCode(const QString &email);
    bool verifyCode(const QString &email, const QString &code);
    void removeCode(const QString &email);

private:
    VerificationCodeManager() = default;

    struct CodeInfo {
        QString code;
        QDateTime createdAt;
    };

    QMap<QString, CodeInfo> m_codes;
};

#endif // VERIFICATIONCODEMANAGER_H
