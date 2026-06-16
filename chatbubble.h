#ifndef CHATBUBBLE_H
#define CHATBUBBLE_H

#include <QWidget>

// ============================================================
// ChatBubble — 聊天气泡控件
//
// 一个小而独立的 Widget，负责渲染单条消息。
// 通过 isSelf 控制样式：
//   true  → 蓝色气泡，右对齐（自己发的消息）
//   false → 白色气泡，左对齐（对方的消息）
//
// 样式使用内联 QSS（Qt Stylesheet），融合 Fusion 风格。
// 没有使用 QListWidget 的自定义 delegate——因为需要灵活
// 控制边距和时间戳布局，直接使用 QWidget + QLabel 更简单。
// ============================================================

class ChatBubble : public QWidget
{
    Q_OBJECT
public:
    explicit ChatBubble(const QString &text, bool isSelf,
                        const QString &timestamp = QString(),
                        QWidget *parent = nullptr);
};

#endif // CHATBUBBLE_H
