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
#include "vault.h"

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
};

QTEST_MAIN(NotesTest)
#include "tst_notes.moc"
