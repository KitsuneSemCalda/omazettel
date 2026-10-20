#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "zettel.h"

// What is known about every note in the notes folder: titles, [[links]],
// backlinks and tags. Built in the background from the files on disk and
// kept current one file at a time as notes are saved.
class NoteIndex : public QObject {
    Q_OBJECT
    // Bumped on every change, so QML bindings that call the functions below
    // can depend on it and re-run.
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool indexing READ indexing NOTIFY indexingChanged)
    Q_PROPERTY(int noteCount READ noteCount NOTIFY changed)

public:
    struct Note {
        QString path;
        QString name;   // File name without .md or .markdown.
        QString title;  // Title or first heading, else name.
        QStringList tags;
        QList<Zettel::LinkUse> links;
    };

    explicit NoteIndex(QObject *parent = nullptr);
    ~NoteIndex() override;

    int revision() const { return m_revision; }
    bool indexing() const { return m_watcher.isRunning(); }
    int noteCount() const { return int(m_notes.size()); }

    // The folder to index; empty clears the index.
    Q_INVOKABLE void setRoot(const QString &root);
    // Rescan everything shortly (after files were created, moved or
    // trashed, or edited outside the app).
    Q_INVOKABLE void scheduleRescan();
    // Rescan now and wait, for tests and startup.
    void rescanNow();
    // Re-read one note (after it was saved).
    Q_INVOKABLE void updateFile(const QString &path);

    // The note a [[target]] written in fromPath points to, or "".
    Q_INVOKABLE QString resolve(const QString &target, const QString &fromPath = QString()) const;
    // Notes that link to path: [{path, title, context}].
    Q_INVOKABLE QVariantList backlinks(const QString &path) const;
    // All notes: [{path, name, title, folder}], by name.
    Q_INVOKABLE QVariantList notes() const;
    // All tags with how many notes use them: [{tag, count}], by tag.
    Q_INVOKABLE QVariantList tags() const;
    // Notes with tag (case does not matter): [{path, name, title, folder}].
    Q_INVOKABLE QVariantList notesWithTag(const QString &tag) const;
    Q_INVOKABLE QStringList tagsOf(const QString &path) const;
    Q_INVOKABLE QString titleOf(const QString &path) const;

    static Note readNote(const QString &path);
    static QHash<QString, Note> scan(const QString &root);

signals:
    void changed();
    void indexingChanged();

private:
    void applyScan();
    void rebuild();
    QVariantMap noteEntry(const Note &note) const;

    QString m_root;
    QHash<QString, Note> m_notes;
    // Lower-case note name -> paths with that name.
    QHash<QString, QStringList> m_byName;
    // Path -> notes linking to it, with the line.
    QHash<QString, QList<QPair<QString, QString>>> m_backlinks;
    // Lower-case tag -> paths, and the spelling first seen.
    QHash<QString, QStringList> m_byTag;
    QHash<QString, QString> m_tagSpelling;
    int m_revision = 0;
    QTimer m_rescanTimer;
    QFutureWatcher<QHash<QString, Note>> m_watcher;
    bool m_rescanAgain = false;
};
