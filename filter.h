#ifndef FILTER_H
#define FILTER_H

#include <QString>
#include <QStringList>

// ============================================================
// Filter — 敏感词过滤 Singleton
//
// 设计考量：
//   当前使用简单的子串匹配（O(n*m) 暴力搜索），敏感词列表
//   约 100 条，匹配效率在短文本场景下可以接受。
//
// 改进方向（如果词库扩展到数千级别）：
//   - 改用 Aho-Corasick 自动机实现 O(n) 多模式匹配
//   - 使用 Trie 树优化部分前缀匹配
//   - 支持通配符/正则表达式模式
//
// 国际化：
//   当前仅支持中文敏感词。如果需要支持多语言，应将词库
//   移至外部文件（如 JSON/YAML/数据库）并按语言加载。
// ============================================================

class Filter
{
public:
    static Filter &instance();

    bool containsSensitiveWords(const QString &text) const;

    QString getSensitiveWordsHint() const;

private:
    Filter();
    ~Filter();

    QStringList m_sensitiveWords;
};

#endif // FILTER_H