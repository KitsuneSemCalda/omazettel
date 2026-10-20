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
