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
| 0 ✅ | Fork and rebrand | New name, binary, app ID, desktop file, icon, version. Upstream remote kept. Build and tests pass. | 1–2 days |
| 1 ✅ | Notes folder and sidebar | `Vault`, `WelcomeView`, `Sidebar`; clicking a note opens it in place (autosave keeps this safe); create, rename, delete notes | ~1 week |
| 2 ✅ | Links and tags | `NoteIndex`, wiki link highlighting, Ctrl+click to follow (creating missing notes), back/forward history, `[[` completion, backlinks panel, quick switcher | 1–1.5 weeks |
| 3 ✅ | Export and templates | Pandoc export profiles, custom templates and reference documents (see below) | 1–1.5 weeks |
| 4 ✅ | Search and outline | Search across notes, outline panel (tags moved into Phase 2) | 3–5 days |
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

## Progress

- **Phase 0 (done, 2026-10-20):** renamed, relicensed GPL-3.0-or-later, builds, 109 upstream tests pass.
- **Phase 1 (done, 2026-10-20):** `Vault` (src/vault.*), `Sidebar.qml`, `NameDialog.qml`,
  `IconButton.qml`, and Backend additions in `src/backend_notes.cpp`
  (`flushAutoSave`, `moveCurrentFile`, `followMovedFile`, `startNewDocument`).
  Notes tests live in `tests/notes/` and run from `bin/test`.
  Not yet: a full-window welcome screen (the sidebar's empty state covers it
  for now).
- **Sidebar fixes (done, 2026-10-20):** new notes go in the clicked folder;
  drag notes and folders between folders (`Vault::movePath`).
- **Phase 2 (done, 2026-10-20), with tags pulled forward from Phase 3:**
  `src/zettel.*` (shared parser: wiki links, tags, front matter),
  `src/noteindex.*` (background index: resolve, backlinks, tags, notes),
  highlighting in `src/markdownhighlighter_notes.cpp`, `NotePanel.qml`
  (tags + backlinks), `NotePicker.qml` (quick switcher and `[[` completer),
  a Tags tab in the sidebar, Ctrl+click / Ctrl+Enter to follow, back and forward.
- **Note templates, auto-created linked notes, link updates (done, 2026-10-20):**
  `src/notetemplates.*` (templates with pandoc YAML headers, placeholders,
  per-template name/folder/existing settings; starters Note, Meeting, Daily,
  Literature note), template picker in the new-note dialog, Ctrl+Shift+D daily
  note, author name setting, muted YAML header in the editor. Missing link
  targets are made on save (not the one being typed). Rename and move rewrite
  links (`NoteIndex::rewriteText/rewriteLinks/movesForFolder`), the open note
  in the editor and the rest on disk.

- **Phase 3 export (done, 2026-10-20):** `src/exporter.*` (profiles as pandoc
  defaults files, starter profiles with PDF engine detection, templates and
  reference documents from pandoc's defaults, new profiles using them,
  background export with pandoc's messages), `ExportDialog.qml`
  (Ctrl+Shift+E, sidebar Export…), `Zettel::plainWikiLinks`. Tested end to end
  with HTML, Word and PDF (pdflatex here; xelatex needs texlive-xetex).
  Later: export a whole folder or a set of notes as one document (Zettlr's
  projects); citations with a bibliography and CSL style.

- **Phase 4 search and outline (done, 2026-10-21):** `NoteIndex::search`
  (all words, "phrases", #tag/tag: filters, accent-insensitive via the
  editor's matcher, title matches first, snippets with highlighted matches),
  a Search tab in the sidebar (Ctrl+Shift+F), `Zettel::outline` and
  `Backend::outline/headingPosition`, an Outline section in the right panel
  marking the current section, `[[Note#Heading]]` and `[[#Heading]]` jumps.

- **Folder export, citations, Oma palette (done, 2026-10-21):**
  `Exporter::folderNotes/saveFolderSettings/combineNotes/exportFolder` with
  `.omazettel-project.yaml` per folder and the export dialog's folder mode;
  `src/citations.*` (BibTeX and CSL JSON, settings in
  `.omazettel/citations.yaml`, reload on change), citeproc on export,
  `[@` completion and citation highlighting, `CitationsDialog.qml`;
  `Backend::omaPalette` and chrome colours in Main.qml.
- **Next (requested 2026-10-21):** storage and collaboration: where notes
  live (local, synced, cloud) and editing by more than one person.

## Export templates (designed 2026-10-20, built in Phase 3)

Modelled on Zettlr's custom templates and reference documents
(docs.zettlr.com/en/export/custom-templates.html), which map directly onto
pandoc's own `--defaults` files, so OmaZettel can use pandoc as is:

- **Profiles** are pandoc defaults files in `<notes>/.omazettel/export/*.yaml`,
  for example `PDF (XeLaTeX).yaml` with `to: pdf`, `pdf-engine: xelatex`,
  `template: ../templates/letter.tex`, plus any pandoc option and `variables:`.
  OmaZettel ships starter profiles (PDF, Word, ODT, HTML, LaTeX) and copies
  them in the first time you export.
- **Templates** (`.tex`, `.html`, …) and **reference documents** (`.docx`,
  `.odt`, `.pptx`) live in `<notes>/.omazettel/templates/`. "New template from
  pandoc's default" runs `pandoc --print-default-template` or
  `--print-default-data-file reference.docx` so you start from something that works.
- A **Templates** screen lists profiles, templates and reference documents;
  opens text ones in the editor (or VS Code) and reference documents in
  LibreOffice or Word; duplicates and renames them.
- **Export dialog** (Ctrl+Shift+E): pick a profile, output path; per-note
  overrides from the note's front matter (title, author, date, any variable),
  since pandoc reads those itself. `[[links]]` are turned into normal text or
  links first (pandoc's `wikilinks_title_after_pipe` extension).
- Because profiles are plain pandoc files, the same export works from a
  terminal: `pandoc -d .omazettel/export/PDF.yaml note.md -o note.pdf`.

Phase order from here: 3 = export and templates, 4 = search across notes and
outline, 5 = polish and packaging.

## Suggested first step
## Suggested first step

Phase 0 and the start of Phase 1: rename, build, and get a working notes sidebar that
opens files in the existing editor. That proves the approach in a few days.
