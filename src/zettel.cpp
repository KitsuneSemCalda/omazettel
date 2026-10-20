#include "zettel.h"

#include <QRegularExpression>
#include <QSet>

namespace Zettel {

namespace {

bool insideRanges(const QList<QPair<int, int>> &ranges, int position) {
    for (const auto &range : ranges) {
        if (position >= range.first && position < range.second)
            return true;
    }
    return false;
}

QString shortened(const QString &line) {
    QString trimmed = line.trimmed();
    if (trimmed.size() > 160)
        trimmed = trimmed.left(157) + QStringLiteral("…");
    return trimmed;
}

int fenceLength(const QString &line, QChar *marker) {
    static const QRegularExpression fenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));
    const QRegularExpressionMatch match = fenceRe.match(line);
    if (!match.hasMatch())
        return 0;
    *marker = match.captured(1).at(0);
    return match.capturedLength(1);
}

QString unquoted(QString value) {
    value = value.trimmed();
    if (value.size() >= 2
            && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))) {
        value = value.mid(1, value.size() - 2);
    }
    return value.trimmed();
}

}

QList<QPair<int, int>> codeRanges(const QString &line) {
    QList<QPair<int, int>> ranges;
    int i = 0;
    while (i < line.size()) {
        if (line.at(i) != QLatin1Char('`')) {
            ++i;
            continue;
        }
        int run = 0;
        while (i + run < line.size() && line.at(i + run) == QLatin1Char('`'))
            ++run;
        const QString ticks(run, QLatin1Char('`'));
        const int close = line.indexOf(ticks, i + run);
        if (close < 0)
            break;
        ranges.append({i, close + run});
        i = close + run;
    }
    return ranges;
}

QList<WikiLink> wikiLinks(const QString &line) {
    QList<WikiLink> links;
    if (!line.contains(QLatin1String("[[")))
        return links;
    static const QRegularExpression wikiRe(
        QStringLiteral("\\[\\[([^\\[\\]\\n|#]+)(?:#([^\\[\\]\\n|]*))?(?:\\|([^\\[\\]\\n]*))?\\]\\]"));
    const auto code = codeRanges(line);
    auto it = wikiRe.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (insideRanges(code, match.capturedStart()))
            continue;
        const QString rawTarget = match.captured(1);
        const QString target = rawTarget.trimmed();
        if (target.isEmpty())
            continue;
        WikiLink link;
        link.start = match.capturedStart();
        link.length = match.capturedLength();
        link.target = target;
        link.heading = match.captured(2).trimmed();
        link.label = match.captured(3).trimmed();
        link.targetStart = match.capturedStart(1) + int(rawTarget.indexOf(target));
        links.append(link);
    }
    return links;
}

QList<TagSpan> tags(const QString &line) {
    QList<TagSpan> found;
    if (!line.contains(QLatin1Char('#')))
        return found;
    // A # that starts a word: not part of a URL fragment, a word or a
    // heading marker ("# Title" has a space after it).
    static const QRegularExpression tagRe(
        QStringLiteral("(?<![\\w#/&:.\\])])#([\\p{L}\\p{N}_][\\p{L}\\p{N}_/\\-]*)"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression digitsOnly(QStringLiteral("^[0-9]+$"));
    const auto code = codeRanges(line);
    // Inside a wiki link, # starts a heading, not a tag.
    QList<QPair<int, int>> skip = code;
    for (const WikiLink &link : wikiLinks(line))
        skip.append({link.start, link.start + link.length});
    auto it = tagRe.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (insideRanges(skip, match.capturedStart()))
            continue;
        QString tag = match.captured(1);
        // Trailing punctuation belongs to the sentence.
        while (tag.endsWith(QLatin1Char('/')) || tag.endsWith(QLatin1Char('-')))
            tag.chop(1);
        if (tag.isEmpty() || digitsOnly.match(tag).hasMatch())
            continue;
        found.append({int(match.capturedStart()), int(tag.size()) + 1, tag});
    }
    return found;
}

QStringList frontMatterTags(const QString &frontMatter) {
    QStringList result;
    const QStringList lines = frontMatter.split(QLatin1Char('\n'));
    static const QRegularExpression keyRe(QStringLiteral("^(tags|keywords)\\s*:\\s*(.*)$"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression itemRe(QStringLiteral("^\\s+-\\s*(.+)$"));
    for (int i = 0; i < lines.size(); ++i) {
        const QRegularExpressionMatch key = keyRe.match(lines.at(i));
        if (!key.hasMatch())
            continue;
        QString value = key.captured(2).trimmed();
        if (!value.isEmpty()) {
            if (value.startsWith(QLatin1Char('[')) && value.endsWith(QLatin1Char(']')))
                value = value.mid(1, value.size() - 2);
            // Commas separate tags; without any, spaces do ("tags: a b").
            const QStringList parts = value.contains(QLatin1Char(','))
                ? value.split(QLatin1Char(','), Qt::SkipEmptyParts)
                : value.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                QString tag = unquoted(part);
                if (tag.startsWith(QLatin1Char('#')))
                    tag.remove(0, 1);
                if (!tag.isEmpty())
                    result.append(tag);
            }
            continue;
        }
        for (int j = i + 1; j < lines.size(); ++j) {
            const QRegularExpressionMatch item = itemRe.match(lines.at(j));
            if (!item.hasMatch())
                break;
            QString tag = unquoted(item.captured(1));
            if (tag.startsWith(QLatin1Char('#')))
                tag.remove(0, 1);
            if (!tag.isEmpty())
                result.append(tag);
        }
    }
    return result;
}

ParsedNote parse(const QString &text) {
    ParsedNote note;
    QSet<QString> seenTags;
    const auto addTag = [&note, &seenTags](const QString &tag) {
        const QString key = tag.toLower();
        if (seenTags.contains(key))
            return;
        seenTags.insert(key);
        note.tags.append(tag);
    };

    QStringList lines = text.split(QLatin1Char('\n'));
    int first = 0;

    // YAML front matter: "---" on the first line through "---" or "...".
    if (!lines.isEmpty() && lines.first().trimmed() == QLatin1String("---")) {
        for (int i = 1; i < lines.size(); ++i) {
            const QString trimmed = lines.at(i).trimmed();
            if (trimmed == QLatin1String("---") || trimmed == QLatin1String("...")) {
                const QString frontMatter = lines.mid(1, i - 1).join(QLatin1Char('\n'));
                for (const QString &tag : frontMatterTags(frontMatter))
                    addTag(tag);
                static const QRegularExpression titleRe(QStringLiteral("^title\\s*:\\s*(.+)$"),
                                                        QRegularExpression::CaseInsensitiveOption
                                                            | QRegularExpression::MultilineOption);
                const QRegularExpressionMatch title = titleRe.match(frontMatter);
                if (title.hasMatch())
                    note.title = unquoted(title.captured(1));
                first = i + 1;
                break;
            }
        }
    }

    static const QRegularExpression headingRe(QStringLiteral("^#\\s+(.+?)\\s*#*\\s*$"));
    QChar fenceMarker;
    int fence = 0;
    for (int i = first; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        QChar marker;
        const int length = fenceLength(line, &marker);
        if (fence > 0) {
            if (length >= fence && marker == fenceMarker && line.trimmed().size() == length)
                fence = 0;
            continue;
        }
        if (length > 0) {
            fence = length;
            fenceMarker = marker;
            continue;
        }

        if (note.title.isEmpty()) {
            const QRegularExpressionMatch heading = headingRe.match(line);
            if (heading.hasMatch())
                note.title = heading.captured(1);
        }
        for (const WikiLink &link : wikiLinks(line))
            note.links.append({link.target, shortened(line)});
        for (const TagSpan &tag : tags(line))
            addTag(tag.tag);
    }
    return note;
}

QString plainWikiLinks(const QString &text) {
    if (!text.contains(QLatin1String("[[")))
        return text;
    QStringList lines = text.split(QLatin1Char('\n'));
    QChar fenceMarker;
    int fence = 0;
    for (QString &line : lines) {
        QChar marker;
        const int length = fenceLength(line, &marker);
        if (fence > 0) {
            if (length >= fence && marker == fenceMarker && line.trimmed().size() == length)
                fence = 0;
            continue;
        }
        if (length > 0) {
            fence = length;
            fenceMarker = marker;
            continue;
        }
        const QList<WikiLink> links = wikiLinks(line);
        for (auto link = links.crbegin(); link != links.crend(); ++link) {
            QString shown = link->label;
            if (shown.isEmpty()) {
                shown = link->target.section(QLatin1Char('/'), -1);
                if (!link->heading.isEmpty())
                    shown += QStringLiteral(" › ") + link->heading;
            }
            line.replace(link->start, link->length, shown);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

}
