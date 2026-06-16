#ifndef UTIL_H
#define UTIL_H

// ============================================================
// util.h — 工具函数（仅头文件，inline 避免 ODR 冲突）
//
// loadPixmap() 支持三种图片来源：
//   1. data: URI — 从 Base64 编码内联数据解码（兼容旧数据）
//   2. HTTP(S) — 从 HTTP 图片服务器异步加载并缓存
//      （使用 QNetworkAccessManager + 同步 QEventLoop）
//   3. 本地路径 — 直接加载本地文件
//
// 关于第 2 种方式（同步 HTTP 在主线程）：
//   这种做法会阻塞 GUI 事件循环 15s（超时时间），在极端
//   情况下会造成界面"冻住"。这是出于简化实现的权衡——
//   如果改为异步 QNetworkReply + 信号驱动，需要引入更多
//   状态管理。在校园网低延迟环境下，图片加载通常很快，
//   阻塞时间可以接受。
//
//   缓存：使用 static QMap 做 LRU-free 的简单缓存，
//   进程生命周期内有效。（注意：static QMap 在多线程场景
//   不安全，但本应用是单线程的。）
// ============================================================

#include <QPixmap>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

inline QPixmap loadPixmap(const QString &data)
{
    if (data.startsWith("data:image")) {
        int commaIdx = data.indexOf(',');
        if (commaIdx > 0) {
            QPixmap pix;
            pix.loadFromData(QByteArray::fromBase64(data.mid(commaIdx + 1).toLatin1()));
            return pix;
        }
    }

    if (data.startsWith("http://") || data.startsWith("https://")) {
        static QMap<QString, QPixmap> s_cache;
        if (s_cache.contains(data))
            return s_cache[data];

        QNetworkAccessManager mgr;
        QUrl url(data);
        QNetworkRequest req(url);
        req.setTransferTimeout(15000);

        QNetworkReply *reply = mgr.get(req);
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);

        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

        timeout.start(15000);
        loop.exec();

        QPixmap pix;
        if (!timeout.isActive()) {
            // timed out
            qDebug("loadPixmap: timeout fetching %s", qPrintable(data));
        } else {
            timeout.stop();
            pix.loadFromData(reply->readAll());
        }

        reply->deleteLater();
        s_cache[data] = pix;
        return pix;
    }

    // 兼容旧的本地路径
    return QPixmap(data);
}

#endif // UTIL_H
