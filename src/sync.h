#pragma once

#include <QDateTime>
#include <functional>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Keeping the notes folder in step with other devices and people, without
// real-time co-editing:
//
//  - Git: the notes folder (or the repository holding it) is saved as
//    versions and synced with a remote: commit, fetch, merge (never rebase),
//    push. When the same note changed on both sides, the merge stops and the
//    note is listed as a conflict to resolve (keep mine, keep theirs, keep
//    both, or edit). Every note's history can be compared and restored.
//  - Synced folders (Syncthing, Nextcloud, Dropbox, ownCloud): the conflict
//    copies those tools leave are listed and resolved the same way.
//
// Git runs without prompts (GIT_TERMINAL_PROMPT=0, ssh BatchMode), so a
// missing login shows as an error rather than a hang.
class Sync : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gitAvailable READ gitAvailable CONSTANT)
    Q_PROPERTY(bool ghAvailable READ ghAvailable CONSTANT)
    Q_PROPERTY(bool isRepo READ isRepo NOTIFY changed)
    Q_PROPERTY(QString repoRoot READ repoRoot NOTIFY changed)
    Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY changed)
    Q_PROPERTY(QString branch READ branch NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // "off" (not a repository), "local" (no remote), "clean", "changes",
    // "error" or "conflict".
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QDateTime lastSync READ lastSync NOTIFY changed)
    Q_PROPERTY(int changeCount READ changeCount NOTIFY changed)
    Q_PROPERTY(QStringList gitConflicts READ gitConflicts NOTIFY changed)
    Q_PROPERTY(QVariantList conflictCopies READ conflictCopies NOTIFY changed)
    Q_PROPERTY(bool autoSync READ autoSync WRITE setAutoSync NOTIFY settingsChanged)
    Q_PROPERTY(int intervalMinutes READ intervalMinutes WRITE setIntervalMinutes NOTIFY settingsChanged)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit Sync(QObject *parent = nullptr);
    ~Sync() override;

    bool gitAvailable() const;
    bool ghAvailable() const;
    bool isRepo() const { return !m_repoRoot.isEmpty(); }
    QString repoRoot() const { return m_repoRoot; }
    QString remoteUrl() const { return m_remoteUrl; }
    QString branch() const { return m_branch; }
    bool busy() const { return m_watcher.isRunning(); }
    QString state() const;
    QString message() const { return m_message; }
    QDateTime lastSync() const { return m_lastSync; }
    int changeCount() const { return m_changeCount; }
    QStringList gitConflicts() const { return m_gitConflicts; }
    QVariantList conflictCopies() const { return m_conflictCopies; }
    bool autoSync() const { return m_autoSync; }
    void setAutoSync(bool enabled);
    int intervalMinutes() const { return m_intervalMinutes; }
    void setIntervalMinutes(int minutes);
    int revision() const { return m_revision; }

    Q_INVOKABLE void setRoot(const QString &notesRoot);
    // Re-reads the repository's status and the folder's conflict copies.
    Q_INVOKABLE void refresh();

    // ── Setting up ──────────────────────────────────────────────────────────
    // Starts version history in the notes folder, with a first version.
    Q_INVOKABLE bool enable();
    Q_INVOKABLE bool setRemote(const QString &url);
    // Makes a GitHub repository with gh, connects it and pushes (in the
    // background). Emits synced().
    Q_INVOKABLE bool createGitHubRepo(const QString &name, bool isPrivate);

    // ── Syncing ─────────────────────────────────────────────────────────────
    // Commit, then fetch, merge and push when there is a remote, in the
    // background. Emits synced(ok, files the merge changed). Save the open
    // note first.
    Q_INVOKABLE bool syncNow();

    // ── Conflicts ───────────────────────────────────────────────────────────
    // {kind: "git"|"copy", path, mine, theirs, tool, label}. For git, path is
    // the note; for a copy, path is the copy.
    Q_INVOKABLE QVariantMap conflict(const QString &path) const;
    // choice: "mine", "theirs", "both" or (git only) "edited".
    Q_INVOKABLE bool resolve(const QString &path, const QString &choice);
    Q_INVOKABLE bool abortMerge();

    // ── History ─────────────────────────────────────────────────────────────
    // A note's saved versions, newest first:
    // [{hash, date (ISO), author, message, path}] (path at that version).
    Q_INVOKABLE QVariantList history(const QString &path) const;
    Q_INVOKABLE QString versionText(const QString &hash, const QString &path) const;
    // Line differences between two texts (TextDiff::lines).
    Q_INVOKABLE QVariantList diff(const QString &before, const QString &after) const;
    Q_INVOKABLE QString lastError() const { return m_error; }

    struct Result {
        bool ok = false;
        QString message;
        QStringList updated;   // Absolute paths the merge changed.
        QStringList conflicts; // Absolute paths.
    };
    // The whole sync, blocking; runs on a worker thread.
    static Result runSync(const QString &repoRoot, const QString &branch, bool hasRemote,
                          const QString &fallbackAuthor);
    static QStringList scanConflictCopies(const QString &root, QVariantList *details);

signals:
    void changed();
    void busyChanged();
    void settingsChanged();
    // Time for an automatic sync: save the open note, then call syncNow().
    void autoSyncDue();
    void synced(bool ok, const QStringList &updatedFiles);

private:
    bool git(const QStringList &args, QString *out = nullptr, QString *err = nullptr,
             int timeoutMs = 15000) const;
    QString relative(const QString &path) const;
    void startWorker(std::function<Result()> work);

    QString m_root;
    QString m_repoRoot;
    QString m_remoteUrl;
    QString m_branch;
    QString m_message;
    QString m_error;
    QDateTime m_lastSync;
    int m_changeCount = 0;
    bool m_lastFailed = false;
    QStringList m_gitConflicts;
    QVariantList m_conflictCopies;
    bool m_autoSync = true;
    int m_intervalMinutes = 5;
    int m_revision = 0;
    QTimer m_timer;
    QFutureWatcher<Result> m_watcher;
};
