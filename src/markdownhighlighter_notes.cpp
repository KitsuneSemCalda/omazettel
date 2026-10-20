// OmaZettel additions to MarkdownHighlighter, kept apart from the upstream
// Omawrite file so merges stay simple.

#include "markdownhighlighter.h"
#include "zettel.h"

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
    if (links.isEmpty() && tagSpans.isEmpty())
        return;

    QTextCharFormat linkFormat = m_linkFormat;
    linkFormat.setFontUnderline(false);
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
    }

    QTextCharFormat tagFormat;
    tagFormat.setForeground(m_linkFormat.foreground());
    for (const Zettel::TagSpan &tag : tagSpans)
        merge(tag.start, tag.length, tagFormat);
}
