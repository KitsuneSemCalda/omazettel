// OmaZettel additions to MarkdownHighlighter, kept apart from the upstream
// Omawrite file so merges stay simple.

#include "markdownhighlighter.h"
#include "zettel.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>

// [[wiki links]] read as links without an underline, their brackets hidden
// unless the caret is in the link, like other markup. #tags take the accent.
namespace {
// The block number of the line closing a YAML front matter block at the top
// of the document, or -1 when there is none.
int frontMatterEnd(const QTextDocument *document) {
    QTextBlock block = document ? document->firstBlock() : QTextBlock();
    if (!block.isValid() || block.text().trimmed() != QLatin1String("---"))
        return -1;
    block = block.next();
    for (int n = 0; block.isValid() && n < 300; ++n, block = block.next()) {
        const QString trimmed = block.text().trimmed();
        if (trimmed == QLatin1String("---") || trimmed == QLatin1String("..."))
            return block.blockNumber();
    }
    return -1;
}
}

void MarkdownHighlighter::setZettelColors(const QHash<QString, QColor> &colors) {
    if (m_zettelColors == colors)
        return;
    m_zettelColors = colors;
    rebuildFormats();
    rehighlightAll();
}

// Theme colours over the formats rebuildFormats() made.
void MarkdownHighlighter::applyZettelColors() {
    const auto color = [this](const char *name) { return m_zettelColors.value(QString::fromLatin1(name)); };
    if (color("heading").isValid())
        m_headingFormat.setForeground(color("heading"));
    if (color("bold").isValid())
        m_boldFormat.setForeground(color("bold"));
    if (color("italic").isValid())
        m_italicFormat.setForeground(color("italic"));
    if (color("link").isValid())
        m_linkFormat.setForeground(color("link"));
    if (color("code").isValid())
        m_codeFormat.setForeground(color("code"));
    if (color("code_background").isValid()) {
        m_codeFormat.setBackground(color("code_background"));
        m_fenceFormat.setBackground(color("code_background"));
    }
    if (color("quote").isValid())
        m_quoteFormat.setForeground(color("quote"));
    if (color("marker").isValid()) {
        m_markerFormat.setForeground(color("marker"));
        m_fenceFormat.setForeground(color("marker"));
    }
    if (color("search").isValid())
        m_searchFormat.setBackground(color("search"));
    if (color("search_current").isValid())
        m_currentSearchFormat.setBackground(color("search_current"));
}

void MarkdownHighlighter::highlightZettel(const QString &text) {
    // The YAML header pandoc reads (title, author, tags, ...) recedes, keys
    // muted, so the note's own text stands out.
    if (currentBlock().blockNumber() < 300) {
        const int end = frontMatterEnd(document());
        if (end >= 0 && currentBlock().blockNumber() <= end) {
            QTextCharFormat value;
            value.setForeground(m_quoteFormat.foreground());
            setFormat(0, text.length(), value);
            const int colon = text.indexOf(QLatin1Char(':'));
            if (text.trimmed() == QLatin1String("---") || text.trimmed() == QLatin1String("..."))
                setFormat(0, text.length(), m_markerFormat);
            else if (colon > 0 && !text.at(0).isSpace())
                setFormat(0, colon + 1, m_markerFormat);
            return;
        }
    }

    const QList<Zettel::WikiLink> links = Zettel::wikiLinks(text);
    const QList<Zettel::TagSpan> tagSpans = Zettel::tags(text);

    // Citations: [@key, p. 4; @other] in the accent, and @key as well.
    if (text.contains(QLatin1Char('@'))) {
        static const QRegularExpression keyRe(QStringLiteral("(?<![\\w@.])@[\\w][\\w:.#$%&+?<>~/-]*"));
        const auto code = Zettel::codeRanges(text);
        QTextCharFormat citeFormat;
        citeFormat.setForeground(m_zettelColors.value(QStringLiteral("citation")).isValid()
                                     ? QBrush(m_zettelColors.value(QStringLiteral("citation")))
                                     : m_linkFormat.foreground());
        auto it = keyRe.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            bool inCode = false;
            for (const auto &range : code)
                inCode = inCode || (match.capturedStart() >= range.first && match.capturedStart() < range.second);
            if (inCode)
                continue;
            QString key = match.captured(0);
            // A sentence's full stop is not part of the key.
            int length = int(key.size());
            while (length > 1 && QStringLiteral(".:;").contains(key.at(length - 1)))
                --length;
            for (int i = int(match.capturedStart()); i < match.capturedStart() + length; ++i) {
                QTextCharFormat merged = format(i);
                merged.merge(citeFormat);
                setFormat(i, 1, merged);
            }
        }
    }
    if (links.isEmpty() && tagSpans.isEmpty())
        return;

    QTextCharFormat linkFormat = m_linkFormat;
    linkFormat.setFontUnderline(false);
    if (m_zettelColors.value(QStringLiteral("wikilink")).isValid())
        linkFormat.setForeground(m_zettelColors.value(QStringLiteral("wikilink")));
    const auto merge = [this](int start, int length, const QTextCharFormat &extra) {
        for (int i = start; i < start + length; ++i) {
            QTextCharFormat merged = format(i);
            merged.merge(extra);
            setFormat(i, 1, merged);
        }
    };

    const int caret = !m_caret.isNull() && m_caret.block() == currentBlock()
        ? m_caret.positionInBlock() : -1;
    for (const Zettel::WikiLink &link : links) {
        merge(link.start, link.length, linkFormat);
        const bool atCaret = caret >= link.start && caret <= link.start + link.length;
        const QTextCharFormat &brackets = atCaret ? m_markerFormat : m_hiddenMarkerFormat;
        setFormat(link.start, 2, brackets);
        setFormat(link.start + link.length - 2, 2, brackets);
        // [[target|label]] reads as its label, like a Markdown link's text,
        // until the caret is in it.
        if (!link.label.isEmpty()) {
            const int pipe = int(text.indexOf(QLatin1Char('|'), link.start + 2));
            if (pipe > 0 && pipe < link.start + link.length - 2)
                setFormat(link.start + 2, pipe + 1 - (link.start + 2), brackets);
        }
    }

    QTextCharFormat tagFormat;
    tagFormat.setForeground(m_zettelColors.value(QStringLiteral("tag")).isValid()
                                ? QBrush(m_zettelColors.value(QStringLiteral("tag")))
                                : m_linkFormat.foreground());
    for (const Zettel::TagSpan &tag : tagSpans)
        merge(tag.start, tag.length, tagFormat);
}
