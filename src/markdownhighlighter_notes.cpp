// OmaZettel additions to MarkdownHighlighter, kept apart from the upstream
// Omawrite file so merges stay simple.

#include "markdownhighlighter.h"
#include "zettel.h"

#include <QTextBlock>

// [[wiki links]] read as links without an underline, their brackets hidden
// unless the caret is in the link, like other markup. #tags take the accent.
void MarkdownHighlighter::highlightZettel(const QString &text) {
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
