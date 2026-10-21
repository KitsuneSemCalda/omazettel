#include "textdiff.h"

#include <QStringList>
#include <QVariantMap>

#include <vector>

namespace TextDiff {

namespace {
QVariantMap line(const char *kind, const QString &text) {
    return {{QStringLiteral("kind"), QString::fromLatin1(kind)}, {QStringLiteral("text"), text}};
}
}

QVariantList lines(const QString &a, const QString &b) {
    const QStringList left = a.split(QLatin1Char('\n'));
    const QStringList right = b.split(QLatin1Char('\n'));

    // The common start and end need no table.
    int prefix = 0;
    while (prefix < left.size() && prefix < right.size() && left.at(prefix) == right.at(prefix))
        ++prefix;
    int suffix = 0;
    while (suffix < left.size() - prefix && suffix < right.size() - prefix
           && left.at(left.size() - 1 - suffix) == right.at(right.size() - 1 - suffix))
        ++suffix;

    QVariantList result;
    for (int i = 0; i < prefix; ++i)
        result.append(line("same", left.at(i)));

    const int n = int(left.size()) - prefix - suffix;
    const int m = int(right.size()) - prefix - suffix;
    if (qint64(n) * qint64(m) > 4000000) {
        // Too large to compare line by line: show it as replaced.
        for (int i = 0; i < n; ++i)
            result.append(line("removed", left.at(prefix + i)));
        for (int j = 0; j < m; ++j)
            result.append(line("added", right.at(prefix + j)));
    } else {
        // Longest common subsequence of the middle lines.
        std::vector<std::vector<int>> table(n + 1, std::vector<int>(m + 1, 0));
        for (int i = n - 1; i >= 0; --i) {
            for (int j = m - 1; j >= 0; --j) {
                table[i][j] = left.at(prefix + i) == right.at(prefix + j)
                    ? table[i + 1][j + 1] + 1
                    : qMax(table[i + 1][j], table[i][j + 1]);
            }
        }
        int i = 0;
        int j = 0;
        while (i < n || j < m) {
            if (i < n && j < m && left.at(prefix + i) == right.at(prefix + j)) {
                result.append(line("same", left.at(prefix + i)));
                ++i;
                ++j;
            } else if (i < n && (j == m || table[i + 1][j] >= table[i][j + 1])) {
                // Removed lines before the lines that replace them.
                result.append(line("removed", left.at(prefix + i)));
                ++i;
            } else {
                result.append(line("added", right.at(prefix + j)));
                ++j;
            }
        }
    }

    for (int k = 0; k < suffix; ++k)
        result.append(line("same", left.at(left.size() - suffix + k)));
    return result;
}

QPair<int, int> counts(const QVariantList &diff) {
    int added = 0;
    int removed = 0;
    for (const QVariant &entry : diff) {
        const QString kind = entry.toMap().value(QStringLiteral("kind")).toString();
        if (kind == QLatin1String("added"))
            ++added;
        else if (kind == QLatin1String("removed"))
            ++removed;
    }
    return {added, removed};
}

}
