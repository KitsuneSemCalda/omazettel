// Tests for OmaZettel's notes features. Kept apart from tst_omazettel.cpp,
// which tracks upstream Omawrite's tests.

#include <QtTest>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>

#include "backend.h"
#include "exporter.h"
#include "noteindex.h"
#include "notetemplates.h"
#include "vault.h"
#include "zettel.h"

namespace {
QString readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

void writeFile(const QString &path, const QByteArray &contents) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}
}

class NotesTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_settingsDirectory.isValid());
        QQuickStyle::setStyle(QStringLiteral("Material"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           m_settingsDirectory.path());
    }

    void init() {
        // Each test starts with no notes folder remembered.
        QSettings().clear();
    }

    // ── Vault ───────────────────────────────────────────────────────────────

    void cleansNames() {
        QCOMPARE(Vault::cleanName(QStringLiteral("  Meeting   notes ")), QStringLiteral("Meeting notes"));
        QCOMPARE(Vault::cleanName(QStringLiteral("a/b\\c")), QStringLiteral("a-b-c"));
        QCOMPARE(Vault::cleanName(QStringLiteral("..hidden")), QStringLiteral("hidden"));
        QCOMPARE(Vault::cleanName(QStringLiteral("   ")), QString());
    }

    void recognizesNoteFiles() {
        QVERIFY(Vault::isNoteFile(QStringLiteral("/a/b.md")));
        QVERIFY(Vault::isNoteFile(QStringLiteral("/a/b.MARKDOWN")));
        QVERIFY(!Vault::isNoteFile(QStringLiteral("/a/b.txt")));
        QVERIFY(!Vault::isNoteFile(QStringLiteral("/a/md")));
    }

    void showsHomeAsTilde() {
        Vault vault;
        QCOMPARE(vault.prettyPath(QDir::homePath() + QStringLiteral("/Notes")), QStringLiteral("~/Notes"));
        QCOMPARE(vault.prettyPath(QStringLiteral("/srv/notes")), QStringLiteral("/srv/notes"));
    }

    void opensAndRemembersFolders() {
        QTemporaryDir one, two;
        Vault vault;
        QVERIFY(!vault.isOpen());
        QVERIFY(!vault.openFolder(one.filePath(QStringLiteral("missing"))));
        QVERIFY(!vault.lastError().isEmpty());

        QVERIFY(vault.openFolder(one.path()));
        QVERIFY(vault.isOpen());
        QCOMPARE(vault.rootPath(), QFileInfo(one.path()).canonicalFilePath());
        QVERIFY(vault.lastError().isEmpty());
        QVERIFY(vault.openFolder(two.path()));
        QCOMPARE(vault.recentFolders().first(), QFileInfo(two.path()).canonicalFilePath());
        QCOMPARE(vault.recentFolders().size(), 2);

        // A new Vault, as on the next start, reopens the last folder.
        Vault reopened;
        QCOMPARE(reopened.rootPath(), QFileInfo(two.path()).canonicalFilePath());

        reopened.closeFolder();
        QVERIFY(!reopened.isOpen());
        Vault afterClose;
        QVERIFY(!afterClose.isOpen());
        QCOMPARE(afterClose.recentFolders().size(), 2);
    }

    void capsRecentFolders() {
        QList<QSharedPointer<QTemporaryDir>> folders;
        Vault vault;
        for (int i = 0; i < 10; ++i) {
            folders.append(QSharedPointer<QTemporaryDir>::create());
            QVERIFY(vault.openFolder(folders.last()->path()));
        }
        QCOMPARE(vault.recentFolders().size(), 8);
        QCOMPARE(vault.recentFolders().first(),
                 QFileInfo(folders.last()->path()).canonicalFilePath());
    }

    void createsNotesWithUniqueNames() {
        QTemporaryDir dir;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));

        const QString first = vault.createNote(vault.rootPath(), QStringLiteral("Idea"));
        QCOMPARE(QFileInfo(first).fileName(), QStringLiteral("Idea.md"));
        QCOMPARE(readFile(first), QStringLiteral("# Idea\n\n"));

        const QString second = vault.createNote(vault.rootPath(), QStringLiteral("Idea"));
        QCOMPARE(QFileInfo(second).fileName(), QStringLiteral("Idea 2.md"));

        const QString markdown = vault.createNote(vault.rootPath(), QStringLiteral("Long.markdown"));
        QCOMPARE(QFileInfo(markdown).fileName(), QStringLiteral("Long.markdown"));

        const QString untitled = vault.createNote(vault.rootPath(), QStringLiteral("  "));
        QCOMPARE(QFileInfo(untitled).fileName(), QStringLiteral("Untitled.md"));

        // Given a note, the new one goes in the folder holding it.
        const QString sub = vault.createFolder(vault.rootPath(), QStringLiteral("Projects"));
        const QString inSub = vault.createNote(vault.createNote(sub, QStringLiteral("A")),
                                               QStringLiteral("B"));
        QCOMPARE(QFileInfo(inSub).absolutePath(), sub);
    }

    void refusesToWorkOutsideTheFolder() {
        QTemporaryDir dir, outside;
        Vault vault;
        QCOMPARE(vault.createNote(QString(), QStringLiteral("x")), QString());
        QVERIFY(vault.openFolder(dir.path()));

        const QString escape = vault.rootPath() + QStringLiteral("/../escape.md");
        QVERIFY(!vault.contains(escape));
        writeFile(outside.filePath(QStringLiteral("other.md")), "x");
        QCOMPARE(vault.renamePath(outside.filePath(QStringLiteral("other.md")), QStringLiteral("y")),
                 QString());
        QVERIFY(!vault.trashPath(outside.filePath(QStringLiteral("other.md"))));
        QVERIFY(!vault.trashPath(vault.rootPath()));
        QVERIFY(QFile::exists(outside.filePath(QStringLiteral("other.md"))));
        // Asked for a folder outside, a new note goes to the vault root.
        QCOMPARE(QFileInfo(vault.createNote(outside.path(), QStringLiteral("n"))).absolutePath(),
                 vault.rootPath());
    }

    void renamesKeepingTheExtension() {
        QTemporaryDir dir;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        const QString note = vault.createNote(vault.rootPath(), QStringLiteral("Draft"));
        vault.createNote(vault.rootPath(), QStringLiteral("Taken"));

        QCOMPARE(QFileInfo(vault.renameTarget(note, QStringLiteral("Final"))).fileName(),
                 QStringLiteral("Final.md"));
        QCOMPARE(vault.renameTarget(note, QStringLiteral("Taken")), QString());
        QVERIFY(vault.lastError().contains(QStringLiteral("already exists")));
        QCOMPARE(vault.renameTarget(note, QStringLiteral("  ")), QString());

        const QString renamed = vault.renamePath(note, QStringLiteral("Final"));
        QVERIFY(QFile::exists(renamed));
        QVERIFY(!QFile::exists(note));
        QCOMPARE(readFile(renamed), QStringLiteral("# Draft\n\n"));

        const QString folder = vault.createFolder(vault.rootPath(), QStringLiteral("Old"));
        QCOMPARE(QFileInfo(vault.renamePath(folder, QStringLiteral("New"))).fileName(),
                 QStringLiteral("New"));
    }

    void movesNotesAndFoldersBetweenFolders() {
        QTemporaryDir dir, outside;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        const QString a = vault.createFolder(vault.rootPath(), QStringLiteral("A"));
        const QString b = vault.createFolder(vault.rootPath(), QStringLiteral("B"));
        const QString note = vault.createNote(a, QStringLiteral("Note"));

        const QString moved = vault.movePath(note, b);
        QCOMPARE(moved, b + QStringLiteral("/Note.md"));
        QVERIFY(QFile::exists(moved));
        QVERIFY(!QFile::exists(note));

        // Folders move with what is in them.
        const QString movedFolder = vault.movePath(b, a);
        QCOMPARE(movedFolder, a + QStringLiteral("/B"));
        QVERIFY(QFile::exists(movedFolder + QStringLiteral("/Note.md")));

        // Back to the top level.
        QCOMPARE(vault.movePath(movedFolder + QStringLiteral("/Note.md"), vault.rootPath()),
                 vault.rootPath() + QStringLiteral("/Note.md"));

        // Not into itself, not over an existing name, not outside.
        QCOMPARE(vault.movePath(a, movedFolder), QString());
        QVERIFY(vault.lastError().contains(QStringLiteral("into itself")));
        QCOMPARE(vault.movePath(a, a), QString());
        vault.createNote(a, QStringLiteral("Note"));
        QCOMPARE(vault.movePath(vault.rootPath() + QStringLiteral("/Note.md"), a), QString());
        QVERIFY(vault.lastError().contains(QStringLiteral("already has")));
        QCOMPARE(vault.movePath(vault.rootPath() + QStringLiteral("/Note.md"), outside.path()), QString());
        QVERIFY(QFile::exists(vault.rootPath() + QStringLiteral("/Note.md")));
    }

    void movesNotesToTheTrash() {
        QTemporaryDir dir;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        const QString note = vault.createNote(vault.rootPath(), QStringLiteral("Gone"));
        if (!vault.trashPath(note))
            QSKIP("No trash is available for the temporary folder on this system.");
        QVERIFY(!QFile::exists(note));
    }

    void remembersTheLastNoteInsideTheFolder() {
        QTemporaryDir dir, outside;
        {
            Vault vault;
            QVERIFY(vault.openFolder(dir.path()));
            const QString note = vault.createNote(vault.rootPath(), QStringLiteral("Last"));
            vault.rememberLastNote(QUrl::fromLocalFile(note));
            writeFile(outside.filePath(QStringLiteral("x.md")), "x");
            vault.rememberLastNote(QUrl::fromLocalFile(outside.filePath(QStringLiteral("x.md"))));
        }
        Vault reopened;
        QCOMPARE(QFileInfo(reopened.lastNotePath()).fileName(), QStringLiteral("Last.md"));
    }

    void showsOnlyMarkdownAndFolders() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a.md")), "a");
        writeFile(dir.filePath(QStringLiteral("b.txt")), "b");
        writeFile(dir.filePath(QStringLiteral(".git/config")), "c");
        writeFile(dir.filePath(QStringLiteral("Sub/c.markdown")), "c");

        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QAbstractItemModel *model = vault.model();
        QTRY_COMPARE(model->rowCount(vault.rootIndex()), 2);
        QStringList names;
        for (int row = 0; row < model->rowCount(vault.rootIndex()); ++row)
            names << vault.displayName(model->index(row, 0, vault.rootIndex()));
        names.sort();
        QCOMPARE(names, (QStringList{QStringLiteral("Sub"), QStringLiteral("a")}));
    }

    // ── Backend additions ───────────────────────────────────────────────────

    void flushesAutoSaveForSavedNotesOnly() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("note.md"));
        Backend backend;
        QScopedPointer<QObject> editor(makeEditor(backend));
        QVERIFY(editor);

        // An untitled draft is never saved behind the writer's back.
        editor->setProperty("text", QStringLiteral("draft"));
        QVERIFY(backend.editorTextChanged());
        QVERIFY(backend.modified());
        QVERIFY(!backend.flushAutoSave());
        QVERIFY(backend.modified());

        backend.saveAs(QUrl::fromLocalFile(path));
        editor->setProperty("text", QStringLiteral("edited"));
        QVERIFY(backend.editorTextChanged());
        QVERIFY(backend.modified());
        QVERIFY(backend.flushAutoSave());
        QVERIFY(!backend.modified());
        QCOMPARE(readFile(path), QStringLiteral("edited"));
    }

    void movesTheOpenNote() {
        QTemporaryDir dir;
        const QString from = dir.filePath(QStringLiteral("old.md"));
        const QString to = dir.filePath(QStringLiteral("sub/new.md"));
        writeFile(from, "body");
        Backend backend;
        QScopedPointer<QObject> editor(makeEditor(backend));
        backend.open(QUrl::fromLocalFile(from));

        QSignalSpy external(&backend, &Backend::externalChangeDetected);
        QVERIFY(backend.moveCurrentFile(QUrl::fromLocalFile(to)));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(to));
        QVERIFY(!QFile::exists(from));
        QCOMPARE(readFile(to), QStringLiteral("body"));

        // Edits keep saving to the new path, and the move was not reported
        // as an outside change.
        editor->setProperty("text", QStringLiteral("after move"));
        QVERIFY(backend.editorTextChanged());
        QVERIFY(backend.flushAutoSave());
        QCOMPARE(readFile(to), QStringLiteral("after move"));
        QTest::qWait(300);
        QCOMPARE(external.count(), 0);

        // Never onto an existing file.
        writeFile(dir.filePath(QStringLiteral("taken.md")), "taken");
        QVERIFY(!backend.moveCurrentFile(QUrl::fromLocalFile(dir.filePath(QStringLiteral("taken.md")))));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("taken.md"))), QStringLiteral("taken"));
    }

    void startsANewDocument() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("note.md"));
        writeFile(path, "body");
        Backend backend;
        QScopedPointer<QObject> editor(makeEditor(backend));
        backend.open(QUrl::fromLocalFile(path));
        backend.startNewDocument();
        QVERIFY(backend.fileUrl().isEmpty());
        QVERIFY(!backend.modified());
        QCOMPARE(editor->property("text").toString(), QString());
        QCOMPARE(readFile(path), QStringLiteral("body"));
    }


    // ── Zettel syntax ───────────────────────────────────────────────────────

    void findsWikiLinks() {
        const auto links = Zettel::wikiLinks(QStringLiteral("See [[Index]], [[Projects/Plan#Goals|the plan]] and `[[code]]`."));
        QCOMPARE(links.size(), 2);
        QCOMPARE(links.at(0).target, QStringLiteral("Index"));
        QCOMPARE(links.at(0).start, 4);
        QCOMPARE(links.at(0).length, 9);
        QCOMPARE(links.at(1).target, QStringLiteral("Projects/Plan"));
        QCOMPARE(links.at(1).heading, QStringLiteral("Goals"));
        QCOMPARE(links.at(1).label, QStringLiteral("the plan"));
        QVERIFY(Zettel::wikiLinks(QStringLiteral("[[ ]] [[]] [single]")).isEmpty());
        // A heading in the same note.
        const auto local = Zettel::wikiLinks(QStringLiteral("[[#Setup]]"));
        QCOMPARE(local.size(), 1);
        QCOMPARE(local.first().target, QString());
        QCOMPARE(local.first().heading, QStringLiteral("Setup"));
        QVERIFY(Zettel::parse(QStringLiteral("[[#Setup]]")).links.isEmpty());
        QCOMPARE(Zettel::plainWikiLinks(QStringLiteral("See [[#Setup]].")), QStringLiteral("See Setup."));
    }

    void findsTags() {
        const auto found = Zettel::tags(QStringLiteral("#idea about #project/client-a, not#this, #123, `#code`, https://x.com/#frag [[Note#Heading]] #ok."));
        QStringList names;
        for (const auto &tag : found)
            names << tag.tag;
        QCOMPARE(names, (QStringList{QStringLiteral("idea"), QStringLiteral("project/client-a"), QStringLiteral("ok")}));
        QCOMPARE(found.first().start, 0);
        QCOMPARE(found.first().length, 5);
        QVERIFY(Zettel::tags(QStringLiteral("# Heading")).isEmpty());
        QVERIFY(Zettel::tags(QStringLiteral("## Heading")).isEmpty());
        QCOMPARE(Zettel::tags(QStringLiteral("#Über")).first().tag, QStringLiteral("Über"));
    }

    void readsFrontMatterTags() {
        QCOMPARE(Zettel::frontMatterTags(QStringLiteral("tags: [a, \"b c\", #d]")),
                 (QStringList{QStringLiteral("a"), QStringLiteral("b c"), QStringLiteral("d")}));
        QCOMPARE(Zettel::frontMatterTags(QStringLiteral("tags: one two")),
                 (QStringList{QStringLiteral("one"), QStringLiteral("two")}));
        QCOMPARE(Zettel::frontMatterTags(QStringLiteral("title: x\ntags:\n  - one\n  - 'two'\nother: y")),
                 (QStringList{QStringLiteral("one"), QStringLiteral("two")}));
        QCOMPARE(Zettel::frontMatterTags(QStringLiteral("keywords: alpha, beta")),
                 (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
    }

    void parsesANote() {
        const Zettel::ParsedNote note = Zettel::parse(QStringLiteral(
            "---\ntitle: \"Front Title\"\ntags: [meta]\n---\n# Heading\n\nText #idea and [[Other]].\n"
            "```\n#notatag [[NotALink]]\n```\nMore #Idea and #new.\n"));
        QCOMPARE(note.title, QStringLiteral("Front Title"));
        QCOMPARE(note.tags, (QStringList{QStringLiteral("meta"), QStringLiteral("idea"), QStringLiteral("new")}));
        QCOMPARE(note.links.size(), 1);
        QCOMPARE(note.links.first().target, QStringLiteral("Other"));
        QCOMPARE(note.links.first().context, QStringLiteral("Text #idea and [[Other]]."));
        QCOMPARE(Zettel::parse(QStringLiteral("no heading")).title, QString());
        QCOMPARE(Zettel::parse(QStringLiteral("intro\n# Late Title #\n")).title, QStringLiteral("Late Title"));
    }

    // ── Note index ──────────────────────────────────────────────────────────

    void indexesLinksBacklinksAndTags() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Index.md")), "# Index\n\nStart at [[Ideas]] and [[Projects/Plan]]. #hub\n");
        writeFile(dir.filePath(QStringLiteral("Ideas.md")), "# Ideas\n\nBack to [[index]]. #idea #Hub\n");
        writeFile(dir.filePath(QStringLiteral("Projects/Plan.md")), "---\ntags: [project]\n---\n# The Plan\n\n[[Ideas]] [[Missing]]\n");
        writeFile(dir.filePath(QStringLiteral("Archive/Plan.md")), "# Old plan\n");
        writeFile(dir.filePath(QStringLiteral(".hidden/Secret.md")), "[[Index]]\n");

        NoteIndex index;
        index.setRoot(dir.path());
        index.rescanNow();
        QCOMPARE(index.noteCount(), 4);

        const QString indexPath = dir.filePath(QStringLiteral("Index.md"));
        const QString ideas = dir.filePath(QStringLiteral("Ideas.md"));
        const QString plan = dir.filePath(QStringLiteral("Projects/Plan.md"));
        QCOMPARE(index.resolve(QStringLiteral("ideas"), indexPath), ideas);
        QCOMPARE(index.resolve(QStringLiteral("Ideas.md"), indexPath), ideas);
        QCOMPARE(index.resolve(QStringLiteral("Projects/Plan"), indexPath), plan);
        QCOMPARE(index.resolve(QStringLiteral("The Plan"), indexPath), plan);
        QCOMPARE(index.resolve(QStringLiteral("Missing"), indexPath), QString());
        // Two notes named Plan: the one in the linking note's folder wins.
        QCOMPARE(index.resolve(QStringLiteral("Plan"), dir.filePath(QStringLiteral("Archive/x.md"))),
                 dir.filePath(QStringLiteral("Archive/Plan.md")));

        const QVariantList back = index.backlinks(ideas);
        QCOMPARE(back.size(), 2);
        QCOMPARE(back.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Index"));
        QVERIFY(back.at(0).toMap().value(QStringLiteral("context")).toString().contains(QStringLiteral("[[Ideas]]")));
        QCOMPARE(index.backlinks(indexPath).size(), 1);
        QCOMPARE(index.titleOf(plan), QStringLiteral("The Plan"));

        // Tags group regardless of case and keep the first spelling.
        const QVariantList tags = index.tags();
        QCOMPARE(tags.size(), 3);
        // Ideas.md comes before Index.md, so its spelling wins.
        QCOMPARE(tags.at(0).toMap().value(QStringLiteral("tag")).toString(), QStringLiteral("Hub"));
        QCOMPARE(tags.at(0).toMap().value(QStringLiteral("count")).toInt(), 2);
        QCOMPARE(index.notesWithTag(QStringLiteral("#HUB")).size(), 2);
        QCOMPARE(index.tagsOf(plan), QStringList{QStringLiteral("project")});

        // Saving a note updates it alone.
        QSignalSpy changed(&index, &NoteIndex::changed);
        writeFile(ideas, "# Ideas\n\nNo links now. #fresh\n");
        index.updateFile(ideas);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(index.backlinks(indexPath).size(), 0);
        QCOMPARE(index.notesWithTag(QStringLiteral("fresh")).size(), 1);
        QFile::remove(ideas);
        index.updateFile(ideas);
        QCOMPARE(index.noteCount(), 3);
    }

    void rescansInTheBackground() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("A.md")), "# A\n");
        NoteIndex index;
        QSignalSpy changed(&index, &NoteIndex::changed);
        index.setRoot(dir.path());
        QTRY_COMPARE(index.noteCount(), 1);
        writeFile(dir.filePath(QStringLiteral("B.md")), "[[A]]\n");
        index.scheduleRescan();
        QTRY_COMPARE(index.noteCount(), 2);
        QCOMPARE(index.backlinks(dir.filePath(QStringLiteral("A.md"))).size(), 1);
        index.setRoot(QString());
        QCOMPARE(index.noteCount(), 0);
    }

    // ── Links in the window ─────────────────────────────────────────────────

    void followsAndCreatesWikiLinks() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Start.md")), "# Start\n\nGo to [[Target]] or [[New idea]].\n");
        writeFile(dir.filePath(QStringLiteral("Sub/Target.md")), "# Target\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        const QUrl start = QUrl::fromLocalFile(dir.filePath(QStringLiteral("Start.md")));
        QMetaObject::invokeMethod(window.get(), "openNote", Q_ARG(QVariant, start));

        // The position of a character inside [[Target]].
        const int inTarget = QStringLiteral("# Start\n\nGo to [[Tar").size();
        QCOMPARE(backend.wikiLinkAt(inTarget), QStringLiteral("Target"));
        QVariant handled;
        QMetaObject::invokeMethod(window.get(), "followLinkAt", Q_RETURN_ARG(QVariant, handled), Q_ARG(QVariant, inTarget));
        QVERIFY(handled.toBool());
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Sub/Target.md"))));

        // Back returns to the start note.
        QMetaObject::invokeMethod(window.get(), "goBack");
        QCOMPARE(backend.fileUrl(), start);
        QMetaObject::invokeMethod(window.get(), "goForward");
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Sub/Target.md"))));
        QMetaObject::invokeMethod(window.get(), "goBack");

        // A link to a note that does not exist yet makes it beside this one.
        const int inNew = QStringLiteral("# Start\n\nGo to [[Target]] or [[New").size();
        QMetaObject::invokeMethod(window.get(), "followLinkAt", Q_RETURN_ARG(QVariant, handled), Q_ARG(QVariant, inNew));
        const QString created = dir.filePath(QStringLiteral("New idea.md"));
        QVERIFY(QFile::exists(created));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(created));
        // From the Note template, with its pandoc header.
        QVERIFY(readFile(created).startsWith(QStringLiteral("---\ntitle: \"New idea\"\n")));
    }

    void completesWikiLinks() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Draft.md")), "# Draft\n\n");
        writeFile(dir.filePath(QStringLiteral("Zettelkasten method.md")), "# Z\n");
        writeFile(dir.filePath(QStringLiteral("Other.md")), "# O\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Draft.md")))));

        auto *editor = window->findChild<QQuickItem *>(QStringLiteral("sourceEditor"));
        auto *completer = window->findChild<QObject *>(QStringLiteral("linkCompleter"));
        QVERIFY(editor && completer);
        editor->setProperty("text", QStringLiteral("# Draft\n\nSee [[zett"));
        editor->setProperty("cursorPosition", editor->property("length"));
        QTRY_VERIFY(completer->property("opened").toBool());
        QCOMPARE(completer->property("query").toString(), QStringLiteral("zett"));
        const QVariantList results = completer->property("results").toList();
        QVERIFY(!results.isEmpty());
        QCOMPARE(results.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Zettelkasten method"));

        QMetaObject::invokeMethod(completer, "accept");
        QCOMPARE(editor->property("text").toString(), QStringLiteral("# Draft\n\nSee [[Zettelkasten method]]"));
        QTRY_VERIFY(!completer->property("opened").toBool());

        // Not inside code, and closed once the link is finished.
        editor->setProperty("text", QStringLiteral("`[[zett"));
        editor->setProperty("cursorPosition", editor->property("length"));
        QTest::qWait(50);
        QVERIFY(!completer->property("opened").toBool());
    }

    void quickSwitcherFindsAndCreatesNotes() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Alpha.md")), "# Alpha\n");
        writeFile(dir.filePath(QStringLiteral("Projects/Beta plan.md")), "# Beta\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        auto *switcher = window->findChild<QObject *>(QStringLiteral("quickSwitcher"));
        QVERIFY(switcher);

        switcher->setProperty("query", QStringLiteral("bpl"));
        QCOMPARE(switcher->property("results").toList().first().toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Beta plan"));
        QMetaObject::invokeMethod(switcher, "accept");
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Projects/Beta plan.md"))));

        switcher->setProperty("query", QStringLiteral("Brand new"));
        QCOMPARE(switcher->property("results").toList().size(), 0);
        QMetaObject::invokeMethod(switcher, "accept");
        // Made in the selected folder, which follows the open note.
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Projects/Brand new.md"))));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Projects/Brand new.md"))));
    }

    void panelShowsBacklinksAndTags() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Hub.md")), "# Hub\n\n#central #topic\n");
        writeFile(dir.filePath(QStringLiteral("Spoke.md")), "# Spoke\n\nUp to [[Hub]].\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setPanelVisible(true);
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        auto *panel = window->findChild<QQuickItem *>(QStringLiteral("notePanel"));
        QVERIFY(panel && panel->isVisible());
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Hub.md")))));
        QCOMPARE(panel->property("backlinks").toList().size(), 1);
        QCOMPARE(panel->property("noteTags").toStringList(), (QStringList{QStringLiteral("central"), QStringLiteral("topic")}));
        QCOMPARE(window->property("contentWidth").toInt(),
                 window->property("width").toInt() - window->property("sidebarWidth").toInt()
                     - window->property("panelWidth").toInt());

        vault.setPanelVisible(false);
        QVERIFY(!panel->isVisible());
        QCOMPARE(window->property("panelWidth").toInt(), 0);
    }


    // ── Note templates ──────────────────────────────────────────────────────

    void rendersTemplates() {
        const QDateTime now(QDate(2026, 10, 20), QTime(9, 5, 7));
        const auto rendered = NoteTemplates::render(QStringLiteral(
            "<!-- omazettel: name=\"{{date}} {{title}}\" folder=\"Meetings\" existing=\"open\" -->\n"
            "---\ntitle: \"{{title}}\"\nauthor: \"{{author}}\"\ndate: \"{{date}}\"\nid: {{id}}\n---\n\n"
            "# {{title}}\n{{date:yyyy/MM}} {{time}} in {{folder}} {{unknown}}\n- {{cursor}}\n"),
            QStringLiteral("Plan \"Q4\""), QStringLiteral("Matt"), QStringLiteral("Work"), now);
        QCOMPARE(rendered.fileName, QStringLiteral("2026-10-20 Plan \"Q4\""));
        QCOMPARE(rendered.folder, QStringLiteral("Meetings"));
        QVERIFY(rendered.openExisting);
        // Quotes are escaped in the YAML header only.
        QCOMPARE(rendered.content, QStringLiteral(
            "---\ntitle: \"Plan \\\"Q4\\\"\"\nauthor: \"Matt\"\ndate: \"2026-10-20\"\nid: 20261020090507\n---\n\n"
            "# Plan \"Q4\"\n2026/10 09:05 in Work {{unknown}}\n- \n"));
        QCOMPARE(rendered.cursor, rendered.content.size() - 1);

        const auto plain = NoteTemplates::render(QStringLiteral("Just {{title}}"), QStringLiteral("a/b"),
                                                 QString(), QString(), now);
        QCOMPARE(plain.fileName, QStringLiteral("a-b"));
        QCOMPARE(plain.cursor, -1);
        QVERIFY(!plain.openExisting);
    }

    void startersCarryAPandocHeader() {
        for (const auto &[name, text] : NoteTemplates::starterTemplates()) {
            const auto rendered = NoteTemplates::render(text, QStringLiteral("T"), QStringLiteral("A"),
                                                        QString(), QDateTime::currentDateTime());
            QVERIFY2(rendered.content.startsWith(QStringLiteral("---\ntitle: ")), qPrintable(name));
            QVERIFY2(rendered.content.contains(QStringLiteral("\nauthor: \"A\"\n")), qPrintable(name));
            QVERIFY2(rendered.content.contains(QStringLiteral("\ndate: \"")), qPrintable(name));
            QVERIFY2(rendered.cursor > 0, qPrintable(name));
            QVERIFY2(!Zettel::parse(rendered.content).title.isEmpty(), qPrintable(name));
        }
    }

    void createsNotesFromTemplates() {
        QTemporaryDir dir;
        NoteTemplates templates;
        templates.setRoot(dir.path());
        templates.setAuthor(QStringLiteral("Test Author"));
        templates.ensureDefaults();
        QCOMPARE(templates.list().size(), 4);
        QVERIFY(templates.needsTitle(QStringLiteral("Meeting")));
        QVERIFY(!templates.needsTitle(QStringLiteral("Daily")));

        // Defaults are written once; a deleted starter stays deleted.
        QFile::remove(templates.pathOf(QStringLiteral("Literature note")));
        templates.ensureDefaults();
        QCOMPARE(templates.list().size(), 3);

        const QString today = QDate::currentDate().toString(Qt::ISODate);
        const QVariantMap meeting = templates.create(QStringLiteral("Meeting"), QStringLiteral("Kickoff"), dir.path());
        QCOMPARE(meeting.value(QStringLiteral("path")).toString(),
                 dir.filePath(QStringLiteral("Meetings/%1 Kickoff.md").arg(today)));
        const QString text = readFile(meeting.value(QStringLiteral("path")).toString());
        QVERIFY(text.contains(QStringLiteral("title: \"Kickoff\"")));
        QVERIFY(text.contains(QStringLiteral("author: \"Test Author\"")));
        QVERIFY(!text.contains(QStringLiteral("omazettel:")));
        QVERIFY(meeting.value(QStringLiteral("cursor")).toInt() > 0);

        // Daily: one per day, opened again rather than duplicated.
        const QVariantMap daily = templates.create(QStringLiteral("Daily"), QString(), dir.path());
        QCOMPARE(daily.value(QStringLiteral("path")).toString(), dir.filePath(QStringLiteral("Daily/%1.md").arg(today)));
        QVERIFY(!daily.value(QStringLiteral("existed")).toBool());
        const QVariantMap again = templates.create(QStringLiteral("Daily"), QString(), dir.path());
        QCOMPARE(again.value(QStringLiteral("path")), daily.value(QStringLiteral("path")));
        QVERIFY(again.value(QStringLiteral("existed")).toBool());

        // An exact name (from a [[link]]) wins over the template's own.
        const QVariantMap exact = templates.create(QStringLiteral("Meeting"), QStringLiteral("Linked"),
                                                   dir.filePath(QStringLiteral("Here")), QStringLiteral("Linked"));
        QCOMPARE(exact.value(QStringLiteral("path")).toString(), dir.filePath(QStringLiteral("Here/Linked.md")));

        QVERIFY(templates.create(QStringLiteral("Nope"), QStringLiteral("x"), dir.path()).isEmpty());
        QVERIFY(!templates.lastError().isEmpty());
    }

    void newNoteDialogUsesTemplates() {
        QTemporaryDir dir;
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        QMetaObject::invokeMethod(window.get(), "newNote", Q_ARG(QVariant, vault.rootPath()));
        auto *dialog = window->findChild<QObject *>(QStringLiteral("nameDialog"));
        QVERIFY(dialog && dialog->property("opened").toBool() || dialog->property("visible").toBool());
        const QVariantList choices = dialog->property("templates").toList();
        QCOMPARE(choices.size(), 5); // Four starters and Blank note.
        QCOMPARE(dialog->property("chosenTemplate").toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Note"));

        auto *field = window->findChild<QObject *>(QStringLiteral("nameField"));
        field->setProperty("text", QStringLiteral("Fresh idea"));
        QMetaObject::invokeMethod(dialog, "finish");
        const QString path = dir.filePath(QStringLiteral("Fresh idea.md"));
        QVERIFY(QFile::exists(path));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(path));
        QVERIFY(readFile(path).startsWith(QStringLiteral("---\ntitle: \"Fresh idea\"")));
        // The caret starts where the template said.
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QCOMPARE(editor->property("cursorPosition").toInt(), readFile(path).indexOf(QStringLiteral("---\n\n")) + 5);

        // Daily note twice: the same note.
        QMetaObject::invokeMethod(window.get(), "openDailyNote");
        const QUrl daily = backend.fileUrl();
        QVERIFY(daily.toLocalFile().endsWith(QDate::currentDate().toString(Qt::ISODate) + QStringLiteral(".md")));
        QMetaObject::invokeMethod(window.get(), "openNote", Q_ARG(QVariant, QUrl::fromLocalFile(path)));
        QMetaObject::invokeMethod(window.get(), "openDailyNote");
        QCOMPARE(backend.fileUrl(), daily);
    }

    // ── Linked notes are made, and links follow renames ─────────────────────

    void createsMissingLinkedNotesOnSave() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Hub.md")), "# Hub\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Hub.md")))));
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));

        // The caret is still in [[Beta]], which may be half typed.
        const QString text = QStringLiteral("# Hub\n\n[[Alpha]] and [[Sub/Gamma]] and [[../Escape]] and [[Beta]]");
        editor->setProperty("text", text);
        editor->setProperty("cursorPosition", text.size() - 3);
        backend.save();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Alpha.md"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Sub/Gamma.md"))));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("Beta.md"))));
        QVERIFY(!QFileInfo::exists(QFileInfo(dir.path()).absolutePath() + QStringLiteral("/Escape.md")));
        QVERIFY(readFile(dir.filePath(QStringLiteral("Alpha.md"))).contains(QStringLiteral("title: \"Alpha\"")));

        // Once the caret leaves it, the next save makes it.
        editor->setProperty("cursorPosition", 0);
        editor->setProperty("text", text + QStringLiteral("."));
        backend.save();
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Beta.md"))));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Hub.md"))));
    }

    void renamingANoteUpdatesLinksToIt() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Sub/Target.md")), "# Target\n\nSelf: [[Target]]\n");
        writeFile(dir.filePath(QStringLiteral("A.md")), "See [[Target]], [[target#Part|label]] and [[Other]].\n```\n[[Target]]\n```\n");
        writeFile(dir.filePath(QStringLiteral("B.md")), "Path: [[Sub/Target]]\n");
        writeFile(dir.filePath(QStringLiteral("Other.md")), "x\n");
        writeFile(dir.filePath(QStringLiteral("Open.md")), "Open links to [[Target]].\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Open.md")))));

        renameThroughDialog(window.get(), dir.filePath(QStringLiteral("Sub/Target.md")), QStringLiteral("Renamed"));

        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Sub/Renamed.md"))));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("A.md"))),
                 QStringLiteral("See [[Renamed]], [[Renamed#Part|label]] and [[Other]].\n```\n[[Target]]\n```\n"));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("B.md"))), QStringLiteral("Path: [[Sub/Renamed]]\n"));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("Sub/Renamed.md"))), QStringLiteral("# Target\n\nSelf: [[Renamed]]\n"));
        // The open note changes in the editor, then saves without a prompt.
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QCOMPARE(editor->property("text").toString(), QStringLiteral("Open links to [[Renamed]].\n"));
        QCOMPARE(backend.status(), QStringLiteral("Updated 5 links in 4 notes"));
        QVERIFY(backend.flushAutoSave());
        QCOMPARE(readFile(dir.filePath(QStringLiteral("Open.md"))), QStringLiteral("Open links to [[Renamed]].\n"));
    }

    void renamingTheOpenNoteUpdatesLinksToIt() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Me.md")), "# Me\n");
        writeFile(dir.filePath(QStringLiteral("Fan.md")), "[[Me]]\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Me.md")))));
        renameThroughDialog(window.get(), dir.filePath(QStringLiteral("Me.md")), QStringLiteral("Myself"));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Myself.md"))));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("Fan.md"))), QStringLiteral("[[Myself]]\n"));
    }

    void movingNotesAndFoldersUpdatesPathLinks() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Old/Deep/Note.md")), "# Note\n");
        writeFile(dir.filePath(QStringLiteral("Links.md")), "[[Note]] and [[Old/Deep/Note]] and [[Deep/Note]]\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        const QString archive = vault.createFolder(vault.rootPath(), QStringLiteral("Archive"));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));

        // Moving a folder: name links stay, path links follow.
        QMetaObject::invokeMethod(window.get(), "moveItem",
                                  Q_ARG(QVariant, dir.filePath(QStringLiteral("Old/Deep"))), Q_ARG(QVariant, archive));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Archive/Deep/Note.md"))));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("Links.md"))),
                 QStringLiteral("[[Note]] and [[Archive/Deep/Note]] and [[Archive/Deep/Note]]\n"));

        // Renaming a folder too.
        m_windowIndex->rescanNow();
        renameThroughDialog(window.get(), dir.filePath(QStringLiteral("Archive")), QStringLiteral("Kept"));
        QCOMPARE(readFile(dir.filePath(QStringLiteral("Links.md"))),
                 QStringLiteral("[[Note]] and [[Kept/Deep/Note]] and [[Kept/Deep/Note]]\n"));
    }


    // ── Export ──────────────────────────────────────────────────────────────

    void preparesTextForPandoc() {
        QCOMPARE(Exporter::prepareText(QStringLiteral(
                     "<!-- omazettel: name=\"x\" -->\nSee [[Index]], [[Plan#Goals]], [[a/B|the b]].\n```\n[[Code]]\n```\n`[[inline]]`")),
                 QStringLiteral("See Index, Plan › Goals, the b.\n```\n[[Code]]\n```\n`[[inline]]`"));
        QCOMPARE(Exporter::formatOf(QStringLiteral("# c\nto: docx\n")), QStringLiteral("docx"));
        QCOMPARE(Exporter::formatOf(QStringLiteral("writer: \"html5+smart\"\n")), QStringLiteral("html5"));
        QCOMPARE(Exporter::formatOf(QStringLiteral("# to: pdf\n")), QStringLiteral("html"));
        QCOMPARE(Exporter::extensionFor(QStringLiteral("latex")), QStringLiteral("tex"));
        QCOMPARE(Exporter::extensionFor(QStringLiteral("html5")), QStringLiteral("html"));
        QCOMPARE(Exporter::extensionFor(QStringLiteral("pdf")), QStringLiteral("pdf"));
        QCOMPARE(Exporter::withTemplate(QStringLiteral("to: pdf\n# template: ${.}/old.tex\n"),
                                        QStringLiteral("template"), QStringLiteral("${.}/../templates/A.tex")),
                 QStringLiteral("to: pdf\ntemplate: ${.}/../templates/A.tex\n"));
        QCOMPARE(Exporter::withTemplate(QStringLiteral("to: docx"), QStringLiteral("reference-doc"), QStringLiteral("r.docx")),
                 QStringLiteral("to: docx\nreference-doc: r.docx\n"));
    }

    void writesStarterProfiles() {
        QTemporaryDir dir;
        Exporter exporter;
        exporter.setRoot(dir.path());
        exporter.ensureDefaults();
        const QVariantList profiles = exporter.profiles();
        QStringList names;
        for (const QVariant &profile : profiles)
            names << profile.toMap().value(QStringLiteral("name")).toString();
        QCOMPARE(names, (QStringList{QStringLiteral("HTML"), QStringLiteral("LaTeX source"),
                                     QStringLiteral("OpenDocument"), QStringLiteral("PDF"), QStringLiteral("Word")}));
        QCOMPARE(profiles.at(3).toMap().value(QStringLiteral("extension")).toString(), QStringLiteral("pdf"));
        QVERIFY(QFileInfo(exporter.templatesFolder()).isDir());
        // Only once: a removed profile stays removed.
        QFile::remove(profiles.at(0).toMap().value(QStringLiteral("path")).toString());
        exporter.ensureDefaults();
        QCOMPARE(exporter.profiles().size(), 4);
        QCOMPARE(exporter.defaultOutputPath(dir.filePath(QStringLiteral("Sub/My note.md")),
                                            profiles.at(4).toMap().value(QStringLiteral("path")).toString()),
                 dir.filePath(QStringLiteral("Sub/My note.docx")));

        const QString none = Exporter::starterProfiles(QString()).first().second;
        QVERIFY(none.contains(QStringLiteral("texlive-xetex")));
        QVERIFY(Exporter::starterProfiles(QStringLiteral("pdflatex")).first().second.contains(QStringLiteral("pdf-engine: pdflatex")));
    }

    void makesTemplatesAndProfiles() {
        QTemporaryDir dir;
        Exporter exporter;
        if (!exporter.pandocAvailable())
            QSKIP("pandoc is not installed.");
        exporter.setRoot(dir.path());
        exporter.ensureDefaults();

        const QString tex = exporter.newTemplate(QStringLiteral("latex"), QStringLiteral("Letter"));
        QCOMPARE(QFileInfo(tex).fileName(), QStringLiteral("Letter.tex"));
        QVERIFY(readFile(tex).contains(QStringLiteral("$body$")));
        const QString docx = exporter.newTemplate(QStringLiteral("docx"), QString());
        QCOMPARE(QFileInfo(docx).fileName(), QStringLiteral("Reference.docx"));
        QVERIFY(QFileInfo(docx).size() > 1000);
        QCOMPARE(exporter.newTemplate(QStringLiteral("nope"), QStringLiteral("x")), QString());
        QVERIFY(!exporter.error().isEmpty());

        const QVariantList templates = exporter.templates();
        QCOMPARE(templates.size(), 2);
        QCOMPARE(templates.at(0).toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("template"));
        QVERIFY(templates.at(0).toMap().value(QStringLiteral("text")).toBool());
        QCOMPARE(templates.at(1).toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("reference"));

        const QString pdfProfile = exporter.profilesFolder() + QStringLiteral("/PDF.yaml");
        const QString letter = exporter.newProfile(QStringLiteral("Letter"), pdfProfile, tex);
        QVERIFY(readFile(letter).contains(QStringLiteral("\ntemplate: ${.}/../templates/Letter.tex\n")));
        // A reference document switches the profile to its own format.
        const QString word = exporter.newProfile(QStringLiteral("Styled Word"), pdfProfile, docx);
        QCOMPARE(Exporter::formatOf(readFile(word)), QStringLiteral("docx"));
        QVERIFY(readFile(word).contains(QStringLiteral("reference-doc: ${.}/../templates/Reference.docx")));
    }

    void exportsNotes() {
        QTemporaryDir dir;
        Exporter exporter;
        if (!exporter.pandocAvailable())
            QSKIP("pandoc is not installed.");
        exporter.setRoot(dir.path());
        exporter.ensureDefaults();
        const QString note = dir.filePath(QStringLiteral("Notes/Plan.md"));
        writeFile(note, "---\ntitle: \"The \\\"Q4\\\" plan\"\nauthor: \"Matt\"\n---\n\nSee [[Index|the index]] and #tag.\n");
        const QString html = dir.filePath(QStringLiteral("out/Plan.html"));

        QSignalSpy finished(&exporter, &Exporter::finished);
        QVERIFY(exporter.exportNote(note, exporter.profilesFolder() + QStringLiteral("/HTML.yaml"), html));
        QVERIFY(exporter.busy());
        QVERIFY(finished.wait(30000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(finished.first().at(2).toString()));
        const QString page = readFile(html);
        QVERIFY(page.contains(QStringLiteral("The “Q4” plan")) || page.contains(QStringLiteral("The &quot;Q4&quot; plan")));
        QVERIFY(page.contains(QStringLiteral("See the index and #tag.")));
        QVERIFY(!page.contains(QStringLiteral("[[")));

        // The editor's text (unsaved changes) can be exported instead.
        finished.clear();
        QVERIFY(exporter.exportText(note, exporter.profilesFolder() + QStringLiteral("/HTML.yaml"), html,
                                    QStringLiteral("# Unsaved\n\nFresh text.\n")));
        QVERIFY(finished.wait(30000));
        QVERIFY(readFile(html).contains(QStringLiteral("Fresh text.")));

        finished.clear();
        const QString docx = dir.filePath(QStringLiteral("Plan.docx"));
        QVERIFY(exporter.exportNote(note, exporter.profilesFolder() + QStringLiteral("/Word.yaml"), docx));
        QVERIFY(finished.wait(30000));
        QVERIFY(finished.first().at(0).toBool());
        QVERIFY(QFileInfo(docx).size() > 1000);

        // A broken profile reports pandoc's error.
        finished.clear();
        writeFile(exporter.profilesFolder() + QStringLiteral("/Broken.yaml"), "to: nosuchformat\n");
        QVERIFY(exporter.exportNote(note, exporter.profilesFolder() + QStringLiteral("/Broken.yaml"),
                                    dir.filePath(QStringLiteral("x.out"))));
        QVERIFY(finished.wait(30000));
        QVERIFY(!finished.first().at(0).toBool());
        QVERIFY(!finished.first().at(2).toString().isEmpty());
    }

    void exportsPdfWhenAnEngineWorks() {
        const QString engine = Exporter::detectPdfEngine();
        if (engine.isEmpty())
            QSKIP("No PDF engine is installed.");
        QTemporaryDir dir;
        Exporter exporter;
        exporter.setRoot(dir.path());
        exporter.ensureDefaults();
        const QString note = dir.filePath(QStringLiteral("Report.md"));
        writeFile(note, "---\ntitle: \"Report\"\nauthor: \"Matt\"\ndate: \"2026-10-20\"\n---\n\n# Intro\n\nText and [[Link]].\n");
        const QString pdf = dir.filePath(QStringLiteral("Report.pdf"));
        QSignalSpy finished(&exporter, &Exporter::finished);
        QVERIFY(exporter.exportNote(note, exporter.profilesFolder() + QStringLiteral("/PDF.yaml"), pdf));
        QVERIFY(finished.wait(120000));
        QVERIFY2(finished.first().at(0).toBool(), qPrintable(finished.first().at(2).toString()));
        QFile file(pdf);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(5) == "%PDF-");
    }

    void exportDialogExportsTheOpenNote() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Open.md")), "# Saved\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        if (!m_windowExporter->pandocAvailable())
            QSKIP("pandoc is not installed.");
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Open.md")))));
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        editor->setProperty("text", QStringLiteral("# Not saved yet\n"));

        QMetaObject::invokeMethod(window.get(), "exportNote", Q_ARG(QVariant, dir.filePath(QStringLiteral("Open.md"))));
        auto *dialog = window->findChild<QObject *>(QStringLiteral("exportDialog"));
        QVERIFY(dialog);
        QVERIFY(dialog->property("profileList").toList().size() >= 5);
        // Pick the HTML profile.
        const QVariantList profiles = dialog->property("profileList").toList();
        for (int i = 0; i < profiles.size(); ++i) {
            if (profiles.at(i).toMap().value(QStringLiteral("name")) == QStringLiteral("HTML")) {
                dialog->setProperty("profileIndex", i);
                QMetaObject::invokeMethod(dialog, "updateOutput");
            }
        }
        QCOMPARE(dialog->property("outputPath").toString(), dir.filePath(QStringLiteral("Open.html")));
        QSignalSpy finished(m_windowExporter.get(), &Exporter::finished);
        QMetaObject::invokeMethod(dialog, "runExport");
        QVERIFY(finished.wait(30000));
        QCOMPARE(dialog->property("resultState").toString(), QStringLiteral("ok"));
        QVERIFY(readFile(dir.filePath(QStringLiteral("Open.html"))).contains(QStringLiteral("Not saved yet")));
        QCOMPARE(m_windowExporter->lastProfile(), profiles.at(0).toMap().value(QStringLiteral("path")).toString());
    }


    // ── Search and outline ──────────────────────────────────────────────────

    void parsesSearchQueries() {
        const auto query = NoteIndex::parseQuery(QStringLiteral("alpha \"exact  phrase\" #Work tag:home/kids beta \"unclosed"));
        QCOMPARE(query.terms, (QStringList{QStringLiteral("alpha"), QStringLiteral("exact phrase"),
                                           QStringLiteral("beta"), QStringLiteral("unclosed")}));
        QCOMPARE(query.tags, (QStringList{QStringLiteral("work"), QStringLiteral("home/kids")}));
        QVERIFY(NoteIndex::parseQuery(QStringLiteral("  ")).terms.isEmpty());
    }

    void searchesAllNotes() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Coffee.md")), "# Coffee\n\nNotes on brewing. #drink\nPour over and espresso.\n");
        writeFile(dir.filePath(QStringLiteral("Tea.md")), "# Tea\n\nGreen tea, brewing at 80°C. Café culture <script>. #drink/hot\n");
        writeFile(dir.filePath(QStringLiteral("Work/Plan.md")), "# Plan\n\nBrewing a plan for coffee sales. #work\n");
        NoteIndex index;
        index.setRoot(dir.path());
        index.rescanNow();

        const auto titles = [](const QVariantList &results) {
            QStringList list;
            for (const QVariant &result : results)
                list << result.toMap().value(QStringLiteral("title")).toString();
            return list;
        };
        // All words must appear; a title match ranks first.
        QCOMPARE(titles(index.search(QStringLiteral("coffee brewing"))),
                 (QStringList{QStringLiteral("Coffee"), QStringLiteral("Plan")}));
        QCOMPARE(titles(index.search(QStringLiteral("\"pour over\""))), QStringList{QStringLiteral("Coffee")});
        QCOMPARE(titles(index.search(QStringLiteral("\"over pour\""))), QStringList());
        // Accents and case are ignored.
        QCOMPARE(titles(index.search(QStringLiteral("CAFE"))), QStringList{QStringLiteral("Tea")});
        // Tags filter, including tags under them.
        QCOMPARE(titles(index.search(QStringLiteral("#drink"))), (QStringList{QStringLiteral("Coffee"), QStringLiteral("Tea")}));
        QCOMPARE(titles(index.search(QStringLiteral("brewing tag:work"))), QStringList{QStringLiteral("Plan")});
        QCOMPARE(index.search(QStringLiteral("")).size(), 0);

        // Snippets: the match in bold, the rest escaped, with its position.
        const QVariantMap tea = index.search(QStringLiteral("café"), QStringLiteral("#ff0000")).first().toMap();
        const QVariantMap snippet = tea.value(QStringLiteral("snippets")).toList().first().toMap();
        const QString html = snippet.value(QStringLiteral("html")).toString();
        QVERIFY2(html.contains(QStringLiteral("<b><font color=\"#ff0000\">Café</font></b>")), qPrintable(html));
        QVERIFY(html.contains(QStringLiteral("&lt;script&gt;")));
        const QString teaText = readFile(dir.filePath(QStringLiteral("Tea.md")));
        QCOMPARE(teaText.mid(snippet.value(QStringLiteral("position")).toInt(),
                             snippet.value(QStringLiteral("length")).toInt()), QStringLiteral("Café"));
    }

    void outlinesNotes() {
        const auto headings = Zettel::outline(QStringLiteral(
            "---\ntitle: x\n# not a heading\n---\n# One\ntext\n## Two ##\n```\n# code\n```\n#nospace\n   ### Three\n####### Seven\n"));
        QCOMPARE(headings.size(), 3);
        QCOMPARE(headings.at(0).text, QStringLiteral("One"));
        QCOMPARE(headings.at(0).level, 1);
        QCOMPARE(headings.at(0).offset, 33);
        QCOMPARE(headings.at(1).text, QStringLiteral("Two"));
        QCOMPARE(headings.at(1).level, 2);
        QCOMPARE(headings.at(2).text, QStringLiteral("Three"));
        QCOMPARE(headings.at(2).level, 3);
    }

    void searchTabOpensResults() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Start.md")), "# Start\n");
        writeFile(dir.filePath(QStringLiteral("Deep/Found.md")), "# Found\n\nSome text\nthe needle is here\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setSidebarVisible(true);
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Start.md")))));

        QMetaObject::invokeMethod(window.get(), "showNotesSearch");
        auto *sidebar = window->findChild<QObject *>(QStringLiteral("sidebar"));
        QCOMPARE(sidebar->property("mode").toString(), QStringLiteral("search"));
        auto *field = window->findChild<QObject *>(QStringLiteral("notesSearchField"));
        auto *view = window->findChild<QObject *>(QStringLiteral("searchView"));
        field->setProperty("text", QStringLiteral("needle"));
        QTRY_COMPARE(view->property("results").toList().size(), 1);

        const QVariantMap result = view->property("results").toList().first().toMap();
        const QVariantMap snippet = result.value(QStringLiteral("snippets")).toList().first().toMap();
        QMetaObject::invokeMethod(window.get(), "openSearchResult",
                                  Q_ARG(QVariant, result.value(QStringLiteral("path"))),
                                  Q_ARG(QVariant, snippet.value(QStringLiteral("position"))),
                                  Q_ARG(QVariant, snippet.value(QStringLiteral("length"))));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Deep/Found.md"))));
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QCOMPARE(editor->property("selectedText").toString(), QStringLiteral("needle"));
    }

    void outlineAndHeadingLinks() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Guide.md")), "# Guide\n\nIntro\n\n## Setup\n\nSteps\n\n## Usage\n\nMore\n");
        writeFile(dir.filePath(QStringLiteral("Links.md")), "See [[Guide#usage]] and [[#Local]].\n\n## Local\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setPanelVisible(true);
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Guide.md")))));
        const QVariantList headings = window->property("outlineHeadings").toList();
        QCOMPARE(headings.size(), 3);
        QCOMPARE(headings.at(1).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Setup"));
        QCOMPARE(backend.headingPosition(QStringLiteral("USAGE")), 33);

        // The panel marks the section the caret is in.
        auto *panel = window->findChild<QObject *>(QStringLiteral("notePanel"));
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        editor->setProperty("cursorPosition", 25);
        QCOMPARE(panel->property("currentHeading").toInt(), 1);

        // Edits update the outline shortly after.
        editor->setProperty("text", QStringLiteral("# Guide\n\n## Only\n"));
        QTRY_COMPARE(window->property("outlineHeadings").toList().size(), 2);
        QVERIFY(backend.flushAutoSave());
        writeFile(dir.filePath(QStringLiteral("Guide.md")), "# Guide\n\nIntro\n\n## Setup\n\nSteps\n\n## Usage\n\nMore\n");

        // [[Note#Heading]] opens the note at the heading; [[#Heading]] jumps
        // within the note.
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Links.md")))));
        const int inLocal = QStringLiteral("See [[Guide#usage]] and [[#Lo").size();
        QVariant handled;
        QMetaObject::invokeMethod(window.get(), "followLinkAt", Q_RETURN_ARG(QVariant, handled), Q_ARG(QVariant, inLocal));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Links.md"))));
        QCOMPARE(editor->property("cursorPosition").toInt(), QStringLiteral("See [[Guide#usage]] and [[#Local]].\n\n").size());

        const int inGuide = QStringLiteral("See [[Gui").size();
        QMetaObject::invokeMethod(window.get(), "followLinkAt", Q_RETURN_ARG(QVariant, handled), Q_ARG(QVariant, inGuide));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(dir.filePath(QStringLiteral("Guide.md"))));
        QCOMPARE(editor->property("cursorPosition").toInt(), 33);
    }

    // ── The window ──────────────────────────────────────────────────────────

    void sidebarOpensNotesInPlace() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("One.md")), "# One\n");
        writeFile(dir.filePath(QStringLiteral("Two.md")), "# Two\n");

        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setSidebarVisible(true);
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);

        auto *sidebar = window->findChild<QQuickItem *>(QStringLiteral("sidebar"));
        QVERIFY(sidebar);
        QVERIFY(sidebar->isVisible());
        QCOMPARE(qRound(sidebar->width()), vault.sidebarWidth());

        const QUrl one = QUrl::fromLocalFile(dir.filePath(QStringLiteral("One.md")));
        const QUrl two = QUrl::fromLocalFile(dir.filePath(QStringLiteral("Two.md")));
        QMetaObject::invokeMethod(window.get(), "openNote", Q_ARG(QVariant, one));
        QCOMPARE(backend.fileUrl(), one);

        // A quick edit is saved, not asked about, on switching notes.
        auto *editor = window->findChild<QObject *>(QStringLiteral("sourceEditor"));
        QVERIFY(editor);
        editor->setProperty("text", QStringLiteral("# One\n\nmore"));
        QVERIFY(backend.modified());
        QMetaObject::invokeMethod(window.get(), "openNote", Q_ARG(QVariant, two));
        QCOMPARE(backend.fileUrl(), two);
        QCOMPARE(readFile(dir.filePath(QStringLiteral("One.md"))), QStringLiteral("# One\n\nmore"));
        auto *unsaved = window->findChild<QObject *>(QStringLiteral("unsavedChangesDialog"));
        QVERIFY(unsaved);
        QVERIFY(!unsaved->property("opened").toBool());

        // The last note is remembered for the next start.
        QCOMPARE(vault.lastNotePath(), dir.filePath(QStringLiteral("Two.md")));
    }

    void newNotesGoInTheSelectedFolder() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Top.md")), "# Top\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setSidebarVisible(true);
        const QString first = vault.createFolder(vault.rootPath(), QStringLiteral("First"));
        const QString second = vault.createFolder(vault.rootPath(), QStringLiteral("Second"));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        auto *sidebar = window->findChild<QQuickItem *>(QStringLiteral("sidebar"));

        // With a note open and nothing else picked, its folder.
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Top.md")))));
        QCOMPARE(sidebar->property("targetFolder").toString(), vault.rootPath());

        // Clicking a folder picks it, even with a note open elsewhere.
        sidebar->setProperty("selectedPath", first);
        QCOMPARE(sidebar->property("targetFolder").toString(), first);
        sidebar->setProperty("selectedPath", second + QStringLiteral("/x.md"));
        QCOMPARE(sidebar->property("targetFolder").toString(), second);
    }

    void movingTheOpenNoteKeepsItOpen() {
        QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("Open.md")), "# Open\n");
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        const QString folder = vault.createFolder(vault.rootPath(), QStringLiteral("Archive"));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        QMetaObject::invokeMethod(window.get(), "openNote",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(dir.filePath(QStringLiteral("Open.md")))));

        QMetaObject::invokeMethod(window.get(), "moveItem",
                                  Q_ARG(QVariant, vault.rootPath() + QStringLiteral("/Open.md")),
                                  Q_ARG(QVariant, folder));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(folder + QStringLiteral("/Open.md")));

        // And moving the folder holding it.
        const QString outer = vault.createFolder(vault.rootPath(), QStringLiteral("Outer"));
        QMetaObject::invokeMethod(window.get(), "moveItem", Q_ARG(QVariant, folder), Q_ARG(QVariant, outer));
        QCOMPARE(backend.fileUrl(), QUrl::fromLocalFile(outer + QStringLiteral("/Archive/Open.md")));
        QVERIFY(QFile::exists(outer + QStringLiteral("/Archive/Open.md")));
    }

    void dragsANoteOntoAFolder() {
        QTemporaryDir dir;
        Backend backend;
        Vault vault;
        QVERIFY(vault.openFolder(dir.path()));
        vault.setSidebarVisible(true);
        const QString folder = vault.createFolder(vault.rootPath(), QStringLiteral("Folder"));
        const QString note = vault.createNote(vault.rootPath(), QStringLiteral("Loose"));
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        auto *quickWindow = qobject_cast<QQuickWindow *>(window.get());
        QVERIFY(quickWindow);
        quickWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(quickWindow));

        QQuickItem *noteRow = nullptr;
        QQuickItem *folderRow = nullptr;
        // The folder model loads and sorts in the background; wait until it
        // has put the folder first, so rows do not move under the drag.
        QTRY_VERIFY((noteRow = rowLabelled(quickWindow, QStringLiteral("Loose")))
                    && (folderRow = rowLabelled(quickWindow, QStringLiteral("Folder")))
                    && folderRow->mapToScene(QPointF()).y() < noteRow->mapToScene(QPointF()).y());
        QTest::qWait(100);

        const QPoint from = noteRow->mapToScene(QPointF(noteRow->width() / 2, noteRow->height() / 2)).toPoint();
        const QPoint to = folderRow->mapToScene(QPointF(folderRow->width() / 2, folderRow->height() / 2)).toPoint();
        QTest::mousePress(quickWindow, Qt::LeftButton, {}, from);
        for (int step = 1; step <= 10; ++step)
            QTest::mouseMove(quickWindow, from + (to - from) * step / 10, 10);
        QTest::mouseRelease(quickWindow, Qt::LeftButton, {}, to, 10);

        QTRY_VERIFY(QFile::exists(folder + QStringLiteral("/Loose.md")));
        QVERIFY(!QFile::exists(note));
    }

    void sidebarHidesAndShowsTheEmptyState() {
        Backend backend;
        Vault vault;
        vault.setSidebarVisible(true);
        QQmlEngine engine;
        QScopedPointer<QObject> window(loadMain(engine, backend, vault));
        QVERIFY(window);
        auto *empty = window->findChild<QQuickItem *>(QStringLiteral("sidebarEmptyState"));
        QVERIFY(empty);
        QVERIFY(empty->isVisible());

        auto *sidebar = window->findChild<QQuickItem *>(QStringLiteral("sidebar"));
        vault.setSidebarVisible(false);
        QVERIFY(!sidebar->isVisible());
        QCOMPARE(window->property("contentWidth").toInt(), window->property("width").toInt());
    }

private:
    // Starts a rename from the sidebar and answers the name dialog.
    void renameThroughDialog(QObject *window, const QString &path, const QString &newName) {
        QMetaObject::invokeMethod(window, "renameItem", Q_ARG(QVariant, path));
        auto *field = window->findChild<QObject *>(QStringLiteral("nameField"));
        auto *dialog = window->findChild<QObject *>(QStringLiteral("nameDialog"));
        QVERIFY(field && dialog);
        field->setProperty("text", newName);
        QMetaObject::invokeMethod(dialog, "finish");
    }

    // The tree row whose label reads text.
    static QQuickItem *rowLabelled(QQuickWindow *window, const QString &text) {
        auto *tree = window->findChild<QQuickItem *>(QStringLiteral("notesTree"));
        return tree ? findText(tree, text) : nullptr;
    }

    // Views do not make their delegates QObject children, so walk the
    // visual item tree.
    static QQuickItem *findText(QQuickItem *item, const QString &text) {
        for (QQuickItem *child : item->childItems()) {
            if (child->inherits("QQuickText") && child->isVisible()
                    && child->property("text").toString() == text)
                return child->parentItem();
            if (QQuickItem *found = findText(child, text))
                return found;
        }
        return nullptr;
    }

    QObject *makeEditor(Backend &backend) {
        QQmlComponent component(&m_engine);
        component.setData("import QtQuick\nTextEdit { property var doc: textDocument }", QUrl());
        QObject *editor = component.create();
        if (editor)
            backend.attachDocument(editor->property("doc").value<QObject *>());
        return editor;
    }

    QObject *loadMain(QQmlEngine &engine, Backend &backend, Vault &vault) {
        const QString mainQmlPath = QFINDTESTDATA("../../src/Main.qml");
        engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
        engine.rootContext()->setContextProperty(QStringLiteral("vault"), &vault);
        m_windowIndex = std::make_unique<NoteIndex>();
        m_windowIndex->setRoot(vault.rootPath());
        m_windowIndex->rescanNow();
        engine.rootContext()->setContextProperty(QStringLiteral("noteIndex"), m_windowIndex.get());
        m_windowTemplates = std::make_unique<NoteTemplates>();
        m_windowTemplates->setRoot(vault.rootPath());
        engine.rootContext()->setContextProperty(QStringLiteral("noteTemplates"), m_windowTemplates.get());
        m_windowExporter = std::make_unique<Exporter>();
        m_windowExporter->setRoot(vault.rootPath());
        engine.rootContext()->setContextProperty(QStringLiteral("exporter"), m_windowExporter.get());
        QQmlComponent component(&engine, QUrl::fromLocalFile(mainQmlPath));
        if (!component.isReady()) {
            qWarning() << component.errorString();
            return nullptr;
        }
        QObject *window = component.create();
        if (!window)
            qWarning() << component.errorString();
        return window;
    }

    QTemporaryDir m_settingsDirectory;
    QQmlEngine m_engine;
    std::unique_ptr<NoteIndex> m_windowIndex;
    std::unique_ptr<NoteTemplates> m_windowTemplates;
    std::unique_ptr<Exporter> m_windowExporter;
};

QTEST_MAIN(NotesTest)
#include "tst_notes.moc"
