#include "noteindex.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent>

#include <algorithm>

namespace {
bool isNote(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("md") || suffix == QLatin1String("markdown");
}

QString stripNoteSuffix(QString name) {
    if (name.endsWith(QLatin1String(".md"), Qt::CaseInsensitive))
        name.chop(3);
    else if (name.endsWith(QLatin1String(".markdown"), Qt::CaseInsensitive))
        name.chop(9);
    return name;
}
}

NoteIndex::NoteIndex(QObject *parent) : QObject(parent) {
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(250);
    connect(&m_rescanTimer, &QTimer::timeout, this, [this]() {
        if (m_root.isEmpty())
            return;
        if (m_watcher.isRunning()) {
            m_rescanAgain = true;
            return;
        }
        const QString root = m_root;
        m_watcher.setFuture(QtConcurrent::run([root]() { return NoteIndex::scan(root); }));
        emit indexingChanged();
    });
    connect(&m_watcher, &QFutureWatcher<QHash<QString, Note>>::finished, this, [this]() {
        applyScan();
        emit indexingChanged();
        if (m_rescanAgain) {
            m_rescanAgain = false;
            m_rescanTimer.start();
        }
    });
}

NoteIndex::~NoteIndex() {
    m_watcher.waitForFinished();
}

void NoteIndex::setRoot(const QString &root) {
    if (m_root == root)
        return;
    m_root = root;
    m_notes.clear();
    rebuild();
    if (!root.isEmpty())
        scheduleRescan();
}

void NoteIndex::scheduleRescan() {
    m_rescanTimer.start();
}

void NoteIndex::rescanNow() {
    m_rescanTimer.stop();
    m_watcher.waitForFinished();
    m_notes = m_root.isEmpty() ? QHash<QString, Note>() : scan(m_root);
    rebuild();
}

void NoteIndex::applyScan() {
    const QHash<QString, Note> result = m_watcher.result();
    // A result for a folder that is no longer open is dropped.
    for (auto it = result.cbegin(); it != result.cend(); ++it) {
        if (!it.key().startsWith(m_root + QLatin1Char('/')))
            return;
    }
    m_notes = result;
    rebuild();
}

NoteIndex::Note NoteIndex::readNote(const QString &path) {
    Note note;
    note.path = path;
    note.name = stripNoteSuffix(QFileInfo(path).fileName());
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const Zettel::ParsedNote parsed = Zettel::parse(QString::fromUtf8(file.readAll()));
        note.title = parsed.title;
        note.tags = parsed.tags;
        note.links = parsed.links;
    }
    if (note.title.isEmpty())
        note.title = note.name;
    return note;
}

QHash<QString, NoteIndex::Note> NoteIndex::scan(const QString &root) {
    QHash<QString, Note> notes;
    // Hidden folders (.git, .omazettel) are skipped, as the sidebar does.
    QDirIterator it(root, {QStringLiteral("*.md"), QStringLiteral("*.markdown")},
                    QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (path.mid(root.size()).contains(QLatin1String("/.")))
            continue;
        notes.insert(path, readNote(path));
    }
    return notes;
}

void NoteIndex::updateFile(const QString &path) {
    if (m_root.isEmpty() || !path.startsWith(m_root + QLatin1Char('/')) || !isNote(path))
        return;
    if (QFileInfo(path).isFile())
        m_notes.insert(path, readNote(path));
    else
        m_notes.remove(path);
    rebuild();
}

void NoteIndex::rebuild() {
    m_byName.clear();
    m_backlinks.clear();
    m_byTag.clear();
    m_tagSpelling.clear();

    for (const Note &note : std::as_const(m_notes))
        m_byName[note.name.toLower()].append(note.path);

    // In path order, so which spelling of a tag shows, and the order of
    // backlinks, never depend on hash order.
    QStringList paths = m_notes.keys();
    std::sort(paths.begin(), paths.end());
    for (const QString &path : std::as_const(paths)) {
        const Note &note = m_notes[path];
        QSet<QString> linked;
        for (const Zettel::LinkUse &link : note.links) {
            const QString target = resolve(link.target, note.path);
            if (target.isEmpty() || target == note.path || linked.contains(target))
                continue;
            linked.insert(target);
            m_backlinks[target].append({note.path, link.context});
        }
        for (const QString &tag : note.tags) {
            const QString key = tag.toLower();
            m_byTag[key].append(note.path);
            if (!m_tagSpelling.contains(key))
                m_tagSpelling.insert(key, tag);
        }
    }
    ++m_revision;
    emit changed();
}

QString NoteIndex::resolve(const QString &rawTarget, const QString &fromPath) const {
    QString target = stripNoteSuffix(rawTarget.trimmed());
    if (target.isEmpty() || m_root.isEmpty())
        return QString();

    QStringList candidates;
    if (target.contains(QLatin1Char('/'))) {
        // A path: relative to the notes folder, matched from the end.
        const QString wanted = QLatin1Char('/') + target.toLower();
        for (const Note &note : m_notes) {
            const QString relative = stripNoteSuffix(note.path.mid(m_root.size())).toLower();
            if (relative.endsWith(wanted))
                candidates.append(note.path);
        }
    } else {
        candidates = m_byName.value(target.toLower());
    }

    if (candidates.isEmpty()) {
        // Fall back to a note whose title matches.
        for (const Note &note : m_notes) {
            if (note.title.compare(target, Qt::CaseInsensitive) == 0)
                candidates.append(note.path);
        }
    }
    if (candidates.isEmpty())
        return QString();
    if (candidates.size() == 1)
        return candidates.first();

    // Several notes share the name: prefer the linking note's folder, then
    // the one nearest the top.
    const QString fromFolder = QFileInfo(fromPath).absolutePath();
    std::sort(candidates.begin(), candidates.end(), [&fromFolder](const QString &a, const QString &b) {
        const bool aHere = QFileInfo(a).absolutePath() == fromFolder;
        const bool bHere = QFileInfo(b).absolutePath() == fromFolder;
        if (aHere != bHere)
            return aHere;
        const int aDepth = int(a.count(QLatin1Char('/')));
        const int bDepth = int(b.count(QLatin1Char('/')));
        return aDepth != bDepth ? aDepth < bDepth : a < b;
    });
    return candidates.first();
}

QVariantMap NoteIndex::noteEntry(const Note &note) const {
    const QString folder = QFileInfo(note.path).absolutePath().mid(m_root.size() + 1);
    return {{QStringLiteral("path"), note.path},
            {QStringLiteral("name"), note.name},
            {QStringLiteral("title"), note.title},
            {QStringLiteral("folder"), folder}};
}

QVariantList NoteIndex::backlinks(const QString &path) const {
    QVariantList result;
    QList<QPair<QString, QString>> sources = m_backlinks.value(path);
    std::sort(sources.begin(), sources.end(), [this](const auto &a, const auto &b) {
        return titleOf(a.first).compare(titleOf(b.first), Qt::CaseInsensitive) < 0;
    });
    for (const auto &source : sources) {
        QVariantMap entry = noteEntry(m_notes.value(source.first));
        entry.insert(QStringLiteral("context"), source.second);
        result.append(entry);
    }
    return result;
}

QVariantList NoteIndex::notes() const {
    QList<Note> sorted = m_notes.values();
    std::sort(sorted.begin(), sorted.end(), [](const Note &a, const Note &b) {
        const int byName = a.name.compare(b.name, Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a.path < b.path;
    });
    QVariantList result;
    for (const Note &note : sorted)
        result.append(noteEntry(note));
    return result;
}

QVariantList NoteIndex::tags() const {
    QStringList keys = m_byTag.keys();
    std::sort(keys.begin(), keys.end());
    QVariantList result;
    for (const QString &key : keys) {
        result.append(QVariantMap{{QStringLiteral("tag"), m_tagSpelling.value(key)},
                                  {QStringLiteral("count"), m_byTag.value(key).size()}});
    }
    return result;
}

QVariantList NoteIndex::notesWithTag(const QString &tag) const {
    QString key = tag.toLower();
    if (key.startsWith(QLatin1Char('#')))
        key.remove(0, 1);
    QList<Note> tagged;
    for (const QString &path : m_byTag.value(key))
        tagged.append(m_notes.value(path));
    std::sort(tagged.begin(), tagged.end(), [](const Note &a, const Note &b) {
        return a.title.compare(b.title, Qt::CaseInsensitive) < 0;
    });
    QVariantList result;
    for (const Note &note : tagged)
        result.append(noteEntry(note));
    return result;
}

QStringList NoteIndex::tagsOf(const QString &path) const {
    return m_notes.value(path).tags;
}

QString NoteIndex::titleOf(const QString &path) const {
    const auto it = m_notes.constFind(path);
    return it == m_notes.cend() ? stripNoteSuffix(QFileInfo(path).fileName()) : it->title;
}
