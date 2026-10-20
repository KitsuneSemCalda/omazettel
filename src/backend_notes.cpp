// OmaZettel additions to Backend. Kept apart from backend.cpp, which tracks
// upstream Omawrite, so merges stay simple.

#include "backend.h"

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
