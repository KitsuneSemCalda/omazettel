#include "sync.h"

#include "notetemplates.h"
#include "textdiff.h"
#include "vault.h"
#include "zettel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QtConcurrent>

namespace {
const QString autoSyncSetting = QStringLiteral("sync/autoSync");
const QString intervalSetting = QStringLiteral("sync/intervalMinutes");

QProcessEnvironment gitEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Never wait on a prompt nobody can see.
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    if (!env.contains(QStringLiteral("GIT_SSH_COMMAND")))
        env.insert(QStringLiteral("GIT_SSH_COMMAND"), QStringLiteral("ssh -o BatchMode=yes"));
    env.insert(QStringLiteral("GIT_MERGE_AUTOEDIT"), QStringLiteral("no"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    return env;
}

// Runs git in dir and waits. Returns true on exit code 0.
bool runGit(const QString &dir, const QStringList &args, QString *out, QString *err,
            int timeoutMs, int *exitCode = nullptr) {
    const QString program = QStandardPaths::findExecutable(QStringLiteral("git"));
    if (program.isEmpty()) {
        if (err)
            *err = QStringLiteral("git is not installed.");
        return false;
    }
    QProcess process;
    process.setWorkingDirectory(dir);
    process.setProcessEnvironment(gitEnvironment());
    process.start(program, args);
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        if (err)
            *err = QStringLiteral("git %1 took too long.").arg(args.value(0));
        if (exitCode)
            *exitCode = -1;
        return false;
    }
    if (out)
        *out = QString::fromUtf8(process.readAllStandardOutput());
    if (err)
        *err = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (exitCode)
        *exitCode = process.exitCode();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString lastLine(const QString &text) {
    const QStringList lines = text.trimmed().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

// A friendlier message for the usual network and login failures.
QString explainRemoteError(const QString &err) {
    const QString lower = err.toLower();
    if (lower.contains(QLatin1String("authentication")) || lower.contains(QLatin1String("permission denied"))
            || lower.contains(QLatin1String("could not read username"))
            || lower.contains(QLatin1String("terminal prompts disabled")))
        return QStringLiteral("Git could not log in to the remote. For GitHub, run `gh auth login` "
                              "then `gh auth setup-git` in a terminal, or set up an SSH key.");
    if (lower.contains(QLatin1String("could not resolve host")) || lower.contains(QLatin1String("network"))
            || lower.contains(QLatin1String("connection")) || lower.contains(QLatin1String("timed out")))
        return QStringLiteral("Could not reach the remote; check the network. Changes are saved locally.");
    if (lower.contains(QLatin1String("repository not found")) || lower.contains(QLatin1String("does not appear")))
        return QStringLiteral("The remote repository was not found; check its address.");
    return lastLine(err);
}

QStringList absolutePaths(const QString &root, const QString &names) {
    QStringList paths;
    for (const QString &name : names.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        paths.append(QDir(root).filePath(name.trimmed()));
    return paths;
}

bool writeText(const QString &path, const QString &text) {
    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(text.toUtf8());
    return file.commit();
}

QString readText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

// Name.md -> "Name (other version).md", numbered if taken.
QString otherVersionPath(const QString &path) {
    const QFileInfo info(path);
    return Vault::uniquePath(info.absolutePath(), info.completeBaseName() + QStringLiteral(" (other version)"),
                             info.suffix());
}

// git -c user.name/email when the repository has none configured.
QStringList identityArgs(const QString &dir, const QString &fallbackAuthor) {
    QString name;
    QString email;
    runGit(dir, {QStringLiteral("config"), QStringLiteral("user.name")}, &name, nullptr, 5000);
    runGit(dir, {QStringLiteral("config"), QStringLiteral("user.email")}, &email, nullptr, 5000);
    QStringList args;
    if (name.trimmed().isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("user.name=%1").arg(fallbackAuthor.isEmpty() ? QStringLiteral("OmaZettel") : fallbackAuthor);
    if (email.trimmed().isEmpty())
        args << QStringLiteral("-c") << QStringLiteral("user.email=%1@%2")
                    .arg(qEnvironmentVariable("USER", QStringLiteral("omazettel")), QSysInfo::machineHostName());
    return args;
}
}

Sync::Sync(QObject *parent) : QObject(parent) {
    QSettings settings;
    m_autoSync = settings.value(autoSyncSetting, true).toBool();
    m_intervalMinutes = qBound(1, settings.value(intervalSetting, 5).toInt(), 240);
    m_timer.setInterval(m_intervalMinutes * 60000);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (m_autoSync && !m_remoteUrl.isEmpty() && !busy() && m_gitConflicts.isEmpty())
            emit autoSyncDue();
    });
    m_timer.start();
    connect(&m_watcher, &QFutureWatcher<Result>::finished, this, [this]() {
        const Result result = m_watcher.result();
        m_lastFailed = !result.ok;
        m_message = result.message;
        if (result.ok)
            m_lastSync = QDateTime::currentDateTime();
        refresh();
        emit busyChanged();
        emit synced(result.ok, result.updated);
    });
}

Sync::~Sync() {
    m_watcher.waitForFinished();
}

bool Sync::gitAvailable() const {
    return !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty();
}

bool Sync::ghAvailable() const {
    return !QStandardPaths::findExecutable(QStringLiteral("gh")).isEmpty();
}

QString Sync::state() const {
    if (!m_gitConflicts.isEmpty())
        return QStringLiteral("conflict");
    if (m_repoRoot.isEmpty())
        return QStringLiteral("off");
    if (m_lastFailed)
        return QStringLiteral("error");
    if (m_remoteUrl.isEmpty())
        return QStringLiteral("local");
    return m_changeCount > 0 ? QStringLiteral("changes") : QStringLiteral("clean");
}

void Sync::setAutoSync(bool enabled) {
    if (m_autoSync == enabled)
        return;
    m_autoSync = enabled;
    QSettings().setValue(autoSyncSetting, enabled);
    emit settingsChanged();
}

void Sync::setIntervalMinutes(int minutes) {
    minutes = qBound(1, minutes, 240);
    if (m_intervalMinutes == minutes)
        return;
    m_intervalMinutes = minutes;
    QSettings().setValue(intervalSetting, minutes);
    m_timer.setInterval(minutes * 60000);
    emit settingsChanged();
}

bool Sync::git(const QStringList &args, QString *out, QString *err, int timeoutMs) const {
    return runGit(m_repoRoot.isEmpty() ? m_root : m_repoRoot, args, out, err, timeoutMs);
}

QString Sync::relative(const QString &path) const {
    return QDir(m_repoRoot).relativeFilePath(path);
}

void Sync::setRoot(const QString &notesRoot) {
    if (m_root == notesRoot)
        return;
    m_root = notesRoot;
    m_lastFailed = false;
    m_message.clear();
    m_lastSync = QDateTime();
    refresh();
}

void Sync::refresh() {
    m_repoRoot.clear();
    m_remoteUrl.clear();
    m_branch.clear();
    m_changeCount = 0;
    m_gitConflicts.clear();
    m_conflictCopies.clear();
    if (!m_root.isEmpty()) {
        QString top;
        if (runGit(m_root, {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")}, &top, nullptr, 5000))
            m_repoRoot = QFileInfo(top.trimmed()).canonicalFilePath();
        if (!m_repoRoot.isEmpty()) {
            QString out;
            if (git({QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("HEAD")}, &out))
                m_branch = out.trimmed();
            if (git({QStringLiteral("remote"), QStringLiteral("get-url"), QStringLiteral("origin")}, &out))
                m_remoteUrl = out.trimmed();
            if (git({QStringLiteral("status"), QStringLiteral("--porcelain")}, &out))
                m_changeCount = int(out.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size());
            if (git({QStringLiteral("diff"), QStringLiteral("--name-only"), QStringLiteral("--diff-filter=U")}, &out))
                m_gitConflicts = absolutePaths(m_repoRoot, out);
        }
        scanConflictCopies(m_root, &m_conflictCopies);
    }
    ++m_revision;
    emit changed();
}

QStringList Sync::scanConflictCopies(const QString &root, QVariantList *details) {
    QStringList copies;
    QDirIterator it(root, {QStringLiteral("*.md"), QStringLiteral("*.markdown")},
                    QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (path.mid(root.size()).contains(QLatin1String("/.")))
            continue;
        QString tool;
        const QString original = Zettel::conflictOriginal(QFileInfo(path).fileName(), &tool);
        if (original.isEmpty())
            continue;
        copies.append(path);
        if (details) {
            const QString originalPath = QFileInfo(path).absoluteDir().filePath(original);
            details->append(QVariantMap{{QStringLiteral("copy"), path},
                                        {QStringLiteral("original"), originalPath},
                                        {QStringLiteral("originalExists"), QFileInfo(originalPath).isFile()},
                                        {QStringLiteral("tool"), tool}});
        }
    }
    copies.sort();
    return copies;
}

bool Sync::enable() {
    m_error.clear();
    if (m_root.isEmpty() || !m_repoRoot.isEmpty())
        return !m_repoRoot.isEmpty();
    QString err;
    if (!runGit(m_root, {QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")}, nullptr, &err, 15000)) {
        m_error = lastLine(err);
        return false;
    }
    const QStringList identity = identityArgs(m_root, NoteTemplates::systemAuthor());
    runGit(m_root, {QStringLiteral("add"), QStringLiteral("-A")}, nullptr, nullptr, 60000);
    runGit(m_root, identity + QStringList{QStringLiteral("commit"), QStringLiteral("--allow-empty"),
                                          QStringLiteral("-m"), QStringLiteral("Start OmaZettel notes history")},
           nullptr, &err, 60000);
    refresh();
    return isRepo();
}

bool Sync::setRemote(const QString &url) {
    m_error.clear();
    if (m_repoRoot.isEmpty())
        return false;
    QString err;
    const QString trimmed = url.trimmed();
    bool ok;
    if (trimmed.isEmpty())
        ok = git({QStringLiteral("remote"), QStringLiteral("remove"), QStringLiteral("origin")}, nullptr, &err);
    else if (m_remoteUrl.isEmpty())
        ok = git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), trimmed}, nullptr, &err);
    else
        ok = git({QStringLiteral("remote"), QStringLiteral("set-url"), QStringLiteral("origin"), trimmed}, nullptr, &err);
    if (!ok)
        m_error = lastLine(err);
    m_lastFailed = false;
    refresh();
    return ok;
}

void Sync::startWorker(std::function<Result()> work) {
    m_watcher.setFuture(QtConcurrent::run(std::move(work)));
    emit busyChanged();
}

bool Sync::createGitHubRepo(const QString &name, bool isPrivate) {
    m_error.clear();
    if (busy() || m_repoRoot.isEmpty() || !m_remoteUrl.isEmpty() || !ghAvailable()) {
        m_error = !ghAvailable() ? QStringLiteral("The GitHub CLI (gh) is not installed.")
                                 : QStringLiteral("This notes folder already has a remote.");
        return false;
    }
    const QString repoRoot = m_repoRoot;
    const QString repoName = Vault::cleanName(name).replace(QLatin1Char(' '), QLatin1Char('-'));
    const QString branch = m_branch;
    startWorker([repoRoot, repoName, isPrivate, branch]() {
        Result result;
        QProcess gh;
        gh.setWorkingDirectory(repoRoot);
        gh.setProcessEnvironment(gitEnvironment());
        gh.start(QStandardPaths::findExecutable(QStringLiteral("gh")),
                 {QStringLiteral("repo"), QStringLiteral("create"), repoName,
                  isPrivate ? QStringLiteral("--private") : QStringLiteral("--public"),
                  QStringLiteral("--source"), repoRoot, QStringLiteral("--remote"), QStringLiteral("origin"),
                  QStringLiteral("--push")});
        if (!gh.waitForFinished(120000) || gh.exitCode() != 0) {
            const QString err = QString::fromUtf8(gh.readAllStandardError());
            result.message = err.contains(QLatin1String("auth login"))
                ? QStringLiteral("gh is not logged in. Run `gh auth login` in a terminal, then try again.")
                : QStringLiteral("Could not create the repository: %1").arg(lastLine(err));
            return result;
        }
        result.ok = true;
        result.message = QStringLiteral("Created %1 on GitHub and pushed the notes.").arg(repoName);
        Q_UNUSED(branch);
        return result;
    });
    return true;
}

bool Sync::syncNow() {
    m_error.clear();
    if (busy() || m_repoRoot.isEmpty())
        return false;
    if (!m_gitConflicts.isEmpty()) {
        m_error = QStringLiteral("Resolve the conflicts first.");
        return false;
    }
    const QString repoRoot = m_repoRoot;
    const QString branch = m_branch;
    const bool hasRemote = !m_remoteUrl.isEmpty();
    const QString author = NoteTemplates::systemAuthor();
    startWorker([repoRoot, branch, hasRemote, author]() {
        return runSync(repoRoot, branch, hasRemote, author);
    });
    return true;
}

Sync::Result Sync::runSync(const QString &repoRoot, const QString &branch, bool hasRemote,
                           const QString &fallbackAuthor) {
    Result result;
    QString out;
    QString err;
    int code = 0;
    const QStringList identity = identityArgs(repoRoot, fallbackAuthor);

    // 1. Save this device's changes as a version.
    if (!runGit(repoRoot, {QStringLiteral("add"), QStringLiteral("-A")}, nullptr, &err, 60000)) {
        result.message = lastLine(err);
        return result;
    }
    runGit(repoRoot, {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--quiet")},
           nullptr, nullptr, 30000, &code);
    bool committed = false;
    if (code == 1) {
        const QString message = QStringLiteral("Notes from %1, %2")
            .arg(QSysInfo::machineHostName(), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        if (!runGit(repoRoot, identity + QStringList{QStringLiteral("commit"), QStringLiteral("-m"), message},
                    nullptr, &err, 60000)) {
            result.message = QStringLiteral("Could not save a version: %1").arg(lastLine(err));
            return result;
        }
        committed = true;
    }
    if (!hasRemote) {
        result.ok = true;
        result.message = committed ? QStringLiteral("Saved a version (no remote to sync with).")
                                   : QStringLiteral("Nothing new to save.");
        return result;
    }

    // 2. Get the other side's versions.
    if (!runGit(repoRoot, {QStringLiteral("fetch"), QStringLiteral("origin")}, nullptr, &err, 90000)) {
        result.message = explainRemoteError(err);
        return result;
    }

    // 3. Merge them in.
    const QString remoteBranch = QStringLiteral("origin/") + branch;
    const bool remoteHasBranch = runGit(repoRoot, {QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                                   QStringLiteral("--quiet"), remoteBranch},
                                        nullptr, nullptr, 10000);
    bool pulled = false;
    if (remoteHasBranch) {
        QString before;
        runGit(repoRoot, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")}, &before, nullptr, 10000);
        if (!runGit(repoRoot, identity + QStringList{QStringLiteral("merge"), QStringLiteral("--no-edit"), remoteBranch},
                    &out, &err, 120000)) {
            QString unmerged;
            runGit(repoRoot, {QStringLiteral("diff"), QStringLiteral("--name-only"), QStringLiteral("--diff-filter=U")},
                   &unmerged, nullptr, 10000);
            result.conflicts = absolutePaths(repoRoot, unmerged);
            if (!result.conflicts.isEmpty()) {
                result.message = result.conflicts.size() == 1
                    ? QStringLiteral("A note changed here and elsewhere. Choose which version to keep.")
                    : QStringLiteral("%1 notes changed here and elsewhere. Choose which versions to keep.")
                          .arg(result.conflicts.size());
            } else {
                result.message = QStringLiteral("Could not merge: %1").arg(lastLine(err + QLatin1Char('\n') + out));
            }
            return result;
        }
        QString changedNames;
        runGit(repoRoot, {QStringLiteral("diff"), QStringLiteral("--name-only"), before.trimmed(), QStringLiteral("HEAD")},
               &changedNames, nullptr, 10000);
        result.updated = absolutePaths(repoRoot, changedNames);
        pulled = !result.updated.isEmpty();
    }

    // 4. Send this side's versions.
    QString ahead = QStringLiteral("1");
    if (remoteHasBranch)
        runGit(repoRoot, {QStringLiteral("rev-list"), QStringLiteral("--count"), remoteBranch + QStringLiteral("..HEAD")},
               &ahead, nullptr, 10000);
    bool pushed = false;
    if (ahead.trimmed() != QLatin1String("0")) {
        if (!runGit(repoRoot, {QStringLiteral("push"), QStringLiteral("-u"), QStringLiteral("origin"), branch},
                    nullptr, &err, 120000)) {
            result.message = explainRemoteError(err);
            result.ok = false;
            return result;
        }
        pushed = true;
    }

    result.ok = true;
    QStringList parts;
    if (pulled)
        parts << (result.updated.size() == 1 ? QStringLiteral("1 file updated")
                                             : QStringLiteral("%1 files updated").arg(result.updated.size()));
    if (pushed)
        parts << QStringLiteral("changes sent");
    result.message = parts.isEmpty() ? QStringLiteral("Up to date.") : QStringLiteral("Synced: ") + parts.join(QStringLiteral(", ")) + QLatin1Char('.');
    return result;
}

QVariantMap Sync::conflict(const QString &path) const {
    if (m_gitConflicts.contains(path)) {
        const QString rel = relative(path);
        QString mine;
        QString theirs;
        const bool hasMine = git({QStringLiteral("show"), QStringLiteral(":2:") + rel}, &mine);
        const bool hasTheirs = git({QStringLiteral("show"), QStringLiteral(":3:") + rel}, &theirs);
        return {{QStringLiteral("kind"), QStringLiteral("git")},
                {QStringLiteral("path"), path},
                {QStringLiteral("label"), rel},
                {QStringLiteral("mine"), hasMine ? mine : QString()},
                {QStringLiteral("theirs"), hasTheirs ? theirs : QString()},
                {QStringLiteral("mineDeleted"), !hasMine},
                {QStringLiteral("theirsDeleted"), !hasTheirs},
                {QStringLiteral("tool"), QStringLiteral("git")}};
    }
    for (const QVariant &entry : m_conflictCopies) {
        const QVariantMap copy = entry.toMap();
        if (copy.value(QStringLiteral("copy")).toString() != path)
            continue;
        const QString original = copy.value(QStringLiteral("original")).toString();
        return {{QStringLiteral("kind"), QStringLiteral("copy")},
                {QStringLiteral("path"), path},
                {QStringLiteral("label"), QDir(m_root).relativeFilePath(original)},
                {QStringLiteral("mine"), readText(original)},
                {QStringLiteral("theirs"), readText(path)},
                {QStringLiteral("mineDeleted"), !QFileInfo(original).isFile()},
                {QStringLiteral("theirsDeleted"), false},
                {QStringLiteral("tool"), copy.value(QStringLiteral("tool"))}};
    }
    return {};
}

bool Sync::resolve(const QString &path, const QString &choice) {
    m_error.clear();
    const QVariantMap item = conflict(path);
    if (item.isEmpty()) {
        m_error = QStringLiteral("That conflict is no longer there.");
        return false;
    }
    const QString mine = item.value(QStringLiteral("mine")).toString();
    const QString theirs = item.value(QStringLiteral("theirs")).toString();

    if (item.value(QStringLiteral("kind")) == QLatin1String("copy")) {
        const QString original = QFileInfo(path).absoluteDir().filePath(
            Zettel::conflictOriginal(QFileInfo(path).fileName()));
        bool ok = false;
        if (choice == QLatin1String("mine")) {
            ok = QFile::moveToTrash(path);
        } else if (choice == QLatin1String("theirs")) {
            // The original goes to the trash, so nothing is lost.
            ok = (!QFileInfo(original).exists() || QFile::moveToTrash(original)) && QFile::rename(path, original);
        } else if (choice == QLatin1String("both")) {
            ok = QFile::rename(path, otherVersionPath(original));
        }
        if (!ok)
            m_error = QStringLiteral("Could not resolve %1.").arg(QFileInfo(path).fileName());
        refresh();
        return ok;
    }

    // Git: write the chosen text, mark it resolved, and finish the merge
    // when nothing is left.
    const QString rel = relative(path);
    QString err;
    bool ok = true;
    if (choice == QLatin1String("mine")) {
        if (item.value(QStringLiteral("mineDeleted")).toBool())
            ok = git({QStringLiteral("rm"), QStringLiteral("--quiet"), rel}, nullptr, &err);
        else
            ok = writeText(path, mine) && git({QStringLiteral("add"), rel}, nullptr, &err);
    } else if (choice == QLatin1String("theirs")) {
        if (item.value(QStringLiteral("theirsDeleted")).toBool())
            ok = git({QStringLiteral("rm"), QStringLiteral("--quiet"), rel}, nullptr, &err);
        else
            ok = writeText(path, theirs) && git({QStringLiteral("add"), rel}, nullptr, &err);
    } else if (choice == QLatin1String("both")) {
        const QString other = otherVersionPath(path);
        ok = writeText(path, mine) && writeText(other, theirs)
            && git({QStringLiteral("add"), rel, relative(other)}, nullptr, &err);
    } else if (choice == QLatin1String("edited")) {
        static const QRegularExpression markers(QStringLiteral("^(<{7}|={7}|>{7})( |$)"),
                                                QRegularExpression::MultilineOption);
        if (markers.match(readText(path)).hasMatch()) {
            m_error = QStringLiteral("The note still has <<<<<<< ======= >>>>>>> lines; remove them first.");
            return false;
        }
        ok = git({QStringLiteral("add"), rel}, nullptr, &err);
    } else {
        return false;
    }
    if (!ok) {
        m_error = err.isEmpty() ? QStringLiteral("Could not write %1.").arg(rel) : lastLine(err);
        return false;
    }

    QString unmerged;
    git({QStringLiteral("diff"), QStringLiteral("--name-only"), QStringLiteral("--diff-filter=U")}, &unmerged);
    if (unmerged.trimmed().isEmpty()) {
        const QStringList identity = identityArgs(m_repoRoot, NoteTemplates::systemAuthor());
        if (!git(identity + QStringList{QStringLiteral("commit"), QStringLiteral("--no-edit")}, nullptr, &err, 60000)) {
            m_error = QStringLiteral("Could not finish the merge: %1").arg(lastLine(err));
            refresh();
            return false;
        }
        m_lastFailed = false;
        m_message = QStringLiteral("Conflicts resolved.");
    }
    refresh();
    return true;
}

bool Sync::abortMerge() {
    QString err;
    const bool ok = git({QStringLiteral("merge"), QStringLiteral("--abort")}, nullptr, &err);
    if (!ok)
        m_error = lastLine(err);
    if (ok) {
        m_lastFailed = false;
        m_message = QStringLiteral("Merge undone; nothing from the other side was taken.");
    }
    refresh();
    return ok;
}

QVariantList Sync::history(const QString &path) const {
    QVariantList result;
    if (m_repoRoot.isEmpty())
        return result;
    QString out;
    if (!git({QStringLiteral("log"), QStringLiteral("--follow"), QStringLiteral("--name-only"),
              QStringLiteral("--format=%x1e%H%x1f%an%x1f%aI%x1f%s"), QStringLiteral("--"), relative(path)}, &out, nullptr, 30000))
        return result;
    for (const QString &block : out.split(QChar(0x1e), Qt::SkipEmptyParts)) {
        const QStringList lines = block.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (lines.isEmpty())
            continue;
        const QStringList fields = lines.first().split(QChar(0x1f));
        if (fields.size() < 4)
            continue;
        result.append(QVariantMap{{QStringLiteral("hash"), fields.at(0)},
                                  {QStringLiteral("author"), fields.at(1)},
                                  {QStringLiteral("date"), fields.at(2)},
                                  {QStringLiteral("message"), fields.at(3)},
                                  {QStringLiteral("path"), lines.size() > 1 ? lines.at(1).trimmed() : relative(path)}});
    }
    return result;
}

QString Sync::versionText(const QString &hash, const QString &path) const {
    QString out;
    const QString rel = QDir::isAbsolutePath(path) ? relative(path) : path;
    git({QStringLiteral("show"), hash + QLatin1Char(':') + rel}, &out, nullptr, 30000);
    return out;
}

QVariantList Sync::diff(const QString &before, const QString &after) const {
    return TextDiff::lines(before, after);
}
