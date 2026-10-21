#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Zettelkasten syntax shared by the highlighter and the note index:
// [[wiki links]], #tags and tags in YAML front matter.
namespace Zettel {

struct WikiLink {
    int start = 0;       // Of the opening [[, in the line.
    int length = 0;      // Through the closing ]].
    QString target;      // Note name or path, without #heading or |label.
    QString heading;     // After #, if any.
    QString label;       // After |, if any.
    int targetStart = 0; // Where target starts in the line.
};

struct TagSpan {
    int start = 0;  // Of the #.
    int length = 0; // Including the #.
    QString tag;    // Without the #.
};

// Within one line of prose. Inline code spans are skipped.
QList<WikiLink> wikiLinks(const QString &line);
QList<TagSpan> tags(const QString &line);

struct LinkUse {
    QString target;
    QString context; // The line it appears on, trimmed and shortened.
};

struct ParsedNote {
    QString title;          // Front matter title, else the first # heading.
    QStringList tags;       // In order of first use, without duplicates.
    QList<LinkUse> links;
};

// A whole note: skips front matter (reading its title and tags), fenced code
// and inline code.
ParsedNote parse(const QString &text);

// Front matter tags or keywords: "tags: [a, b]", "tags: a, b" or a list.
QStringList frontMatterTags(const QString &frontMatter);

// The text with every [[link]] (outside code) replaced by its label, or its
// target (and heading), for export, where links between notes mean nothing.
QString plainWikiLinks(const QString &text);

struct Heading {
    int level = 1;
    QString text;
    int offset = 0; // Of the line, in the text.
};

// The # headings of a note, outside front matter and fenced code.
QList<Heading> outline(const QString &text);

// Copies sync tools make when two devices change a file at once:
// Syncthing (Note.sync-conflict-20261021-101010-ABCDEFG.md), Nextcloud and
// Dropbox (Note (conflicted copy 2026-10-21 101010).md, Note (Matt's
// conflicted copy 2026-10-21).md) and ownCloud (Note_conflict-20261021-101010.md).
// Returns the file name of the original ("Note.md"), or "" if fileName is
// not such a copy; tool names which made it.
QString conflictOriginal(const QString &fileName, QString *tool = nullptr);

// The inline code spans of a line, as [start, end) pairs.
QList<QPair<int, int>> codeRanges(const QString &line);

}
