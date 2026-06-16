// 引入聊天气泡的头文件
#include "chatbubble.h"
// 引入标签控件，用于显示消息文本和时间
#include <QLabel>
// 引入水平布局管理器，用于排列气泡和弹性空间
#include <QHBoxLayout>
// 引入垂直布局管理器，用于排列气泡文本和时间标签
#include <QVBoxLayout>

// 构造函数：创建一个聊天气泡组件
// text: 消息文本内容
// isSelf: 是否为当前用户发送的消息（true=自己，false=对方）
// timestamp: 消息的时间字符串（如 "14:30"）
// parent: 父组件
ChatBubble::ChatBubble(const QString &text, bool isSelf,
                       const QString &timestamp, QWidget *parent)
    : QWidget(parent)   // 调用基类QWidget的构造函数
{
    // 设置气泡整体背景透明，让父容器的背景色透出
    setStyleSheet("background: transparent;");

    // 创建水平行布局，用于在一行内排列气泡和两侧的弹性空间
    auto *rowLayout = new QHBoxLayout(this);
    // 取消行布局的外边距，使气泡紧贴边缘
    rowLayout->setContentsMargins(0, 0, 0, 0);
    // 取消行布局内控件之间的间距
    rowLayout->setSpacing(0);

    // 创建包裹容器，用于容纳气泡文本和时间标签（垂直排列）
    auto *wrapper = new QWidget();
    // 设置包裹容器背景透明
    wrapper->setStyleSheet("background: transparent;");
    // 创建包裹容器的垂直布局
    auto *wrapperLayout = new QVBoxLayout(wrapper);
    // 取消包裹布局的外边距
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    // 设置包裹布局内控件间距为4像素
    wrapperLayout->setSpacing(4);

    // 创建消息文本标签（即气泡本体）
    auto *bubble = new QLabel(text);
    // 限制气泡最大宽度为260像素，防止消息过长撑出屏幕
    bubble->setMaximumWidth(260);
    // 启用自动换行，长文本会自动折行显示
    bubble->setWordWrap(true);
    // 设置气泡的样式：内边距、圆角、字号、文字颜色和背景色
    bubble->setStyleSheet(
        QString(
            "QLabel {"
            "  padding: 10px 14px;"
            "  border-radius: 12px;"
            "  font-size: 14px;"
            "  color: %1;"
            "  background-color: %2;"
            "}"
        // 自己发的消息：白色文字 + 蓝色背景；对方消息：深色文字 + 白色背景
        ).arg(isSelf ? "white" : "#333333",
              isSelf ? "#3498db" : "#ffffff")
    );
    // 将气泡标签添加到包裹布局中
    wrapperLayout->addWidget(bubble);

    // 如果传入了时间字符串（非空），则在气泡下方添加时间标签
    if (!timestamp.isEmpty()) {
        // 创建时间显示标签
        auto *timeLbl = new QLabel(timestamp);
        // 设置时间标签的颜色（自己发的偏深灰，对方的偏浅灰），字体小一号
        timeLbl->setStyleSheet(
            QString("color: %1; font-size: 10px; background: transparent;")
                .arg(isSelf ? "#7f8c8d" : "#95a5a6")
        );
        // 如果是自己发的消息，时间标签右对齐
        if (isSelf)
            timeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        // 将时间标签添加到包裹布局（位于气泡文本下方）
        wrapperLayout->addWidget(timeLbl);
    }

    // 根据消息方向排列行布局
    if (isSelf) {
        // 自己发的消息：左侧添加弹性空间把气泡推到右侧（右对齐）
        rowLayout->addStretch();
        rowLayout->addWidget(wrapper);
    } else {
        // 对方发的消息：右侧添加弹性空间把气泡推到左侧（左对齐）
        rowLayout->addWidget(wrapper);
        rowLayout->addStretch();
    }
}
