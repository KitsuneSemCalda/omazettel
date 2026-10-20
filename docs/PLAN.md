# OmaZettel: plan (a notes app built on OmaWrite)

Base: `omacom/omawrite` v0.7.2 (`c1ea454`), cloned to `~/Development/omawrite`.
Goal: a native Qt/QML notes app for Omarchy. It works on a folder of plain `.md`
files and adds a notes sidebar, wiki links and backlinks, search across notes,
tags and pandoc export, all styled to fit with the Oma apps.

## What OmaWrite gives us

About 4,800 lines of app code, plus a 3,500-line QtTest suite.

| File | Lines | What it does |
|---|---|---|
| `src/backend.cpp/.h` | 1,535 | **One document per process**: open, save, autosave, crash recovery, warnings about outside changes, Omarchy theme loading (`colors.toml`, updated live), fonts, text scale, print, copy as HTML |
| `src/Main.qml` | 1,321 | The single window: a `TextEdit` in a `Flickable`, smooth scrolling, a footer of icon buttons, search and replace bar, about 20 `Shortcut`s |
| `src/markdownhighlighter.cpp/.h` | 726 | `QSyntaxHighlighter` with live-preview behavior: hides markup symbols unless the cursor is next to them; handles headings, emphasis, code, fences, quotes, links, search hits |
| `src/systemtheme.cpp/.h` | 245 | Dark/light setting and text scale, read through D-Bus |
| Dialogs and widgets (`*.qml`) | ~700 | Unsaved changes, outside change, shortcuts, font picker, buttons |

Key facts that shape the plan:
- `Backend` is **one document per process**. "New window" starts another process.
  Tabs would mean splitting `Backend`. That's the most invasive change, so it's deferred.
- Theme colors, the font and the text scale are already handled the Omarchy way. Keep them.
- Upstream is very active: 175 commits since Sept 1, 2026. **Keep upstream files
  lightly touched**, put new features in new files, and merge upstream regularly.
- `Ctrl+P` is print, `Ctrl+O` is open and `Ctrl+K` is insert link, so new shortcuts have to avoid them.

## Architecture: what we add

New C++ (each class exposed to QML as a context property, like `backend` is today):

| Class | Job |
|---|---|
| `Vault` | The current notes folder: root path, recent folders (`QSettings`), a tree model of `.md` files and folders (`QFileSystemModel` filtered, or a small custom model), create/rename/move/delete notes, watch the folder for changes |
| `NoteIndex` | Background index (`QtConcurrent`, already a dependency): titles, headings, `[[links]]`, `#tags`, words. Answers backlinks, unresolved links, tag lists and full-text search with snippets. Updates one file at a time when it changes. |
| `Exporter` | Runs `pandoc` through `QProcess`. Export profiles (PDF, DOCX, HTML) stored in `<vault>/.omazettel/pandoc/` (defaults file plus template), so they're plain files you can edit in VS Code too. Turns `[[links]]` into normal links with pandoc's `wikilinks_title_after_pipe`. |

Changes to upstream files (kept small):

- `markdownhighlighter.*`: add `InlineKind::WikiLink` and tag highlighting. Extend
  `inlineMarkup()` so the "find what the cursor is on" code understands `[[...]]` automatically.
- `backend.*`: a `linkTargetAt()` that tells wiki links from URLs, plus a way to open a note
  by vault path. Nothing else; `open()` already handles unsaved changes.
- `Main.qml`: wrap the current editor area in a `SplitView` with a left sidebar and an
  optional right panel. Add the new shortcuts.

New QML, following OmaShow's patterns (`SlideNavigator`, `Inspector`, `CommandPalette`,
`StartCentre`, `Theme.qml`):

| File | Like in OmaShow | Job |
|---|---|---|
| `Sidebar.qml` | `SlideNavigator` | Notes tree (Qt 6 `TreeView`), new note and folder, rename, drag to move |
| `SidePanel.qml` | `Inspector` | Backlinks, outline (headings), tags for the current note |
| `QuickSwitcher.qml` | `CommandPalette` | Fuzzy-find a note by title; create it if missing |
| `CommandPalette.qml` | `CommandPalette` | Every action, keyboard-first |
| `VaultSearch.qml` | none | Search across notes with snippets; click to jump to the match |
| `ExportDialog.qml` | `ExportWorkspace` (simplified) | Choose a profile, output path, open when done |
| `WelcomeView.qml` | `StartCentre` | Pick or create a notes folder, recent folders |
| `LinkCompleter.qml` | none | Popup after `[[`: finish a note name |

Proposed shortcuts: `Ctrl+Shift+O` quick switcher · `Ctrl+Shift+P` command palette ·
`Ctrl+Shift+F` search all notes · `Ctrl+\` toggle sidebar · `Ctrl+Shift+E` export ·
`Ctrl+click` / `Ctrl+Enter` follow link · `Alt+Left` back.

## Phases

Estimates assume one person working with AI help.

| # | Phase | Delivers | Estimate |
|---|---|---|---|
| 0 | Fork and rebrand | New name, binary, app ID, desktop file, icon, version. Upstream remote kept. Build and tests pass. | 1–2 days |
| 1 | Notes folder and sidebar | `Vault`, `WelcomeView`, `Sidebar`; clicking a note opens it in place (autosave keeps this safe); create, rename, delete notes | ~1 week |
| 2 | Links | `NoteIndex`, wiki link highlighting, Ctrl+click to follow (creating missing notes), back/forward history, `[[` completion, backlinks panel, quick switcher | 1–1.5 weeks |
| 3 | Search and tags | Search across notes, tag list and filter, outline panel | 3–5 days |
| 4 | Pandoc export | `Exporter`, profiles, export dialog, status and error reporting | 3–5 days |
| 5 | Polish and packaging | Command palette, settings, PKGBUILD, README, screenshots, tests for each new class | ~1 week |
| | **Version 1 total** | | **~4–6 weeks** |

After version 1, in rough order of value:
- **Tabs or multiple notes in one window**: split `Backend` into an app-level object and a
  `Document` object (autosave, recovery, file watching, highlighter per document). 1–2 weeks.
  It touches upstream's busiest code, so merges get harder afterwards.
- Pasting and dropping images into `<vault>/attachments/`, shown inline.
- Kanban view of a note's `- [ ]` task lists, still stored as plain Markdown.
- Showing math and Mermaid as images inline (likely by running a command-line tool).
- Graph view.

## Risks and decisions

- **Name and branding:** the app is **OmaZettel** (binary `omazettel`, app ID `omazettel`). Make a new icon, and say in the README that it is an independent fork of Omawrite, not made by omacom.
  The MIT license means keeping the original copyright notice in `LICENSE`.
- **License:** copying QML from OmaShow (GPL-3.0) makes the whole app GPL-3.0.
  Either accept that, which is fine for an open-source release, or reuse only the patterns and stay MIT.
- **Upstream drift:** schedule a regular `git merge upstream/master`. The fewer upstream
  lines changed, the cheaper this stays. Tabs are the change most likely to make merges hard.
- **Large notes folders:** index in the background and update one file at a time.
  Test with about 5,000 notes before release.
- **Live preview has limits:** the highlighter styles text but can't show images or
  tables inline in a `TextEdit`. Plan for that, or treat it as later work.

## Suggested first step

Phase 0 and the start of Phase 1: rename, build, and get a working notes sidebar that
opens files in the existing editor. That proves the approach in a few days.
