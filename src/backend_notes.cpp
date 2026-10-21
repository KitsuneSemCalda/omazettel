// OmaZettel additions to Backend. Kept apart from backend.cpp, which tracks
// upstream Omawrite, so merges stay simple.

#include "backend.h"
#include "zettel.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QRegularExpression>

#include <QDir>
#include <QFile>
#include <QFileInfo>

// Saves now what auto-save would save shortly, so switching to another note
// does not stop to ask about changes auto-save was about to keep anyway.
// Returns true when nothing is left unsaved. Untitled drafts, and files whose
// auto-save is off (outside changes, unusual encodings), still go through the
// unsaved-changes prompt.
bool Backend::flushAutoSave() {
    if (!m_modified)
        return true;
    if (!m_fileUrl.isLocalFile() || m_autoSaveSuspended || m_autoSaveHeld)
        return false;
    autoSave();
    return !m_modified;
}

// Renames or moves the open note's file and keeps editing it at its new path,
// without the file watcher reporting the old path as deleted.
bool Backend::moveCurrentFile(const QUrl &newUrl) {
    if (!m_fileUrl.isLocalFile() || !newUrl.isLocalFile())
        return false;

    const QString from = m_fileUrl.toLocalFile();
    const QString to = newUrl.toLocalFile();
    if (QFileInfo(from).absoluteFilePath() == QFileInfo(to).absoluteFilePath())
        return true;

    if (m_modified && !flushAutoSave()) {
        setStatus(QStringLiteral("Save %1 before renaming it.").arg(fileName()));
        return false;
    }
    if (QFileInfo::exists(to)) {
        setStatus(QStringLiteral("%1 already exists.").arg(QFileInfo(to).fileName()));
        return false;
    }

    QDir().mkpath(QFileInfo(to).absolutePath());
    unwatchFiles();
    if (!QFile::rename(from, to)) {
        watchCurrentFile();
        setStatus(QStringLiteral("Could not rename %1.").arg(fileName()));
        return false;
    }

    setFileUrl(newUrl);
    watchCurrentFile();
    setStatus(QStringLiteral("Renamed to %1").arg(fileName()));
    return true;
}

// Points the open note at the path it now has after something else moved it,
// such as renaming the folder that holds it.
void Backend::followMovedFile(const QUrl &newUrl) {
    if (!newUrl.isLocalFile())
        return;
    unwatchFiles();
    setFileUrl(newUrl);
    watchCurrentFile();
}

// Replaces the open note with an empty untitled document in this window, for
// when the note is moved to the trash. Callers deal with unsaved changes first.
void Backend::startNewDocument() {
    unwatchFiles();
    m_autoSaveTimer.stop();
    m_autoSaveSuspended = false;
    loadDocumentText(QString());
    clearRecovery();
    setKnownFileContents(QByteArray(), false);
    setFileUrl(QUrl());
    setModified(false);
    setStatus(QString());
}

QString Backend::wikiLinkAt(int position) const {
    if (!m_document)
        return QString();
    const QTextBlock block = m_document->findBlock(position);
    if (!block.isValid())
        return QString();
    const int column = position - block.position();
    for (const Zettel::WikiLink &link : Zettel::wikiLinks(block.text())) {
        if (column >= link.start && column <= link.start + link.length)
            return link.target;
    }
    return QString();
}

QVariantMap Backend::wikiQueryAt(int position) const {
    if (!m_document || insideFence(position))
        return {};
    const QTextBlock block = m_document->findBlock(position);
    if (!block.isValid())
        return {};
    const QString before = block.text().left(position - block.position());
    static const QRegularExpression openRe(QStringLiteral("\\[\\[([^\\[\\]|#\\n]*)$"));
    const QRegularExpressionMatch match = openRe.match(before);
    if (!match.hasMatch())
        return {};
    // Not inside inline code.
    for (const auto &range : Zettel::codeRanges(before + QStringLiteral("`"))) {
        if (match.capturedStart() >= range.first && match.capturedStart() < range.second)
            return {};
    }
    const QString after = block.text().mid(position - block.position());
    return {{QStringLiteral("query"), match.captured(1)},
            {QStringLiteral("start"), block.position() + int(match.capturedStart(1))},
            {QStringLiteral("closed"), after.startsWith(QStringLiteral("]]"))}};
}

QString Backend::wikiHeadingAt(int position) const {
    if (!m_document)
        return QString();
    const QTextBlock block = m_document->findBlock(position);
    if (!block.isValid())
        return QString();
    const int column = position - block.position();
    for (const Zettel::WikiLink &link : Zettel::wikiLinks(block.text())) {
        if (column >= link.start && column <= link.start + link.length)
            return link.heading;
    }
    return QString();
}

QVariantList Backend::outline() const {
    QVariantList result;
    if (!m_document)
        return result;
    for (const Zettel::Heading &heading : Zettel::outline(m_document->toPlainText())) {
        result.append(QVariantMap{{QStringLiteral("level"), heading.level},
                                  {QStringLiteral("text"), heading.text},
                                  {QStringLiteral("position"), heading.offset}});
    }
    return result;
}

int Backend::headingPosition(const QString &text) const {
    if (!m_document || text.trimmed().isEmpty())
        return -1;
    const QString wanted = text.trimmed();
    for (const Zettel::Heading &heading : Zettel::outline(m_document->toPlainText())) {
        if (heading.text.compare(wanted, Qt::CaseInsensitive) == 0)
            return heading.offset;
    }
    return -1;
}
