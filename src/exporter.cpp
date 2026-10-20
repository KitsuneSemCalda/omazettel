#include "exporter.h"

#include "vault.h"
#include "zettel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString exportSubfolder = QStringLiteral(".omazettel/export");
const QString templatesSubfolder = QStringLiteral(".omazettel/templates");
const QString lastProfileSetting = QStringLiteral("export/lastProfile");
const QStringList textTemplateSuffixes{QStringLiteral("tex"), QStringLiteral("latex"),
                                       QStringLiteral("html"), QStringLiteral("htm"),
                                       QStringLiteral("typ"), QStringLiteral("txt"),
                                       QStringLiteral("md"), QStringLiteral("css"),
                                       QStringLiteral("csl"), QStringLiteral("lua")};
const QStringList referenceSuffixes{QStringLiteral("docx"), QStringLiteral("odt"),
                                    QStringLiteral("pptx")};

QString readText(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

bool writeText(const QString &path, const QString &text) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(text.toUtf8());
    return file.commit();
}

QString pandocPath() {
    return QStandardPaths::findExecutable(QStringLiteral("pandoc"));
}
}

Exporter::Exporter(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(180000);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        if (m_process)
            m_process->kill();
    });
}

Exporter::~Exporter() {
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

bool Exporter::pandocAvailable() const {
    return !pandocPath().isEmpty();
}

QString Exporter::profilesFolder() const {
    return m_root.isEmpty() ? QString() : QDir(m_root).filePath(exportSubfolder);
}

QString Exporter::templatesFolder() const {
    return m_root.isEmpty() ? QString() : QDir(m_root).filePath(templatesSubfolder);
}

QString Exporter::lastProfile() const {
    return QSettings().value(lastProfileSetting).toString();
}

void Exporter::setLastProfile(const QString &path) {
    if (path == lastProfile())
        return;
    QSettings().setValue(lastProfileSetting, path);
    reload();
}

void Exporter::setRoot(const QString &notesRoot) {
    if (m_root == notesRoot)
        return;
    m_root = notesRoot;
    reload();
}

// XeLaTeX handles any Unicode text and system fonts, so it is the first
// choice, but only when its format file is built (texlive-xetex installed).
QString Exporter::detectPdfEngine() {
    const auto works = [](const QString &engine, const QString &format, const QString &kpseEngine) {
        if (QStandardPaths::findExecutable(engine).isEmpty())
            return false;
        const QString kpsewhich = QStandardPaths::findExecutable(QStringLiteral("kpsewhich"));
        if (kpsewhich.isEmpty())
            return true;
        QProcess probe;
        probe.start(kpsewhich, {QStringLiteral("-engine=") + kpseEngine, format});
        return probe.waitForFinished(5000) && !probe.readAllStandardOutput().trimmed().isEmpty();
    };
    if (works(QStringLiteral("xelatex"), QStringLiteral("xelatex.fmt"), QStringLiteral("xetex")))
        return QStringLiteral("xelatex");
    if (works(QStringLiteral("pdflatex"), QStringLiteral("pdflatex.fmt"), QStringLiteral("pdftex")))
        return QStringLiteral("pdflatex");
    if (!QStandardPaths::findExecutable(QStringLiteral("typst")).isEmpty())
        return QStringLiteral("typst");
    if (!QStandardPaths::findExecutable(QStringLiteral("weasyprint")).isEmpty())
        return QStringLiteral("weasyprint");
    return QString();
}

QList<QPair<QString, QString>> Exporter::starterProfiles(const QString &pdfEngine) {
    const QString header = QStringLiteral(
        "# An OmaZettel export profile: a pandoc defaults file.\n"
        "# Any pandoc option works here: https://pandoc.org/MANUAL.html#defaults-files\n"
        "# ${.} is this folder, so ${.}/../templates/<file> is a file in the templates folder.\n"
        "# A note's own YAML header (title, author, date, ...) fills the template.\n");
    QString pdf = header
        + QStringLiteral("\nto: pdf\n");
    if (pdfEngine.isEmpty()) {
        pdf += QStringLiteral(
            "# No PDF engine was found. Install one, then set it here, for example:\n"
            "#   sudo pacman -S texlive-xetex    then    pdf-engine: xelatex\n"
            "pdf-engine: xelatex\n");
    } else {
        pdf += QStringLiteral("pdf-engine: %1\n").arg(pdfEngine);
        if (pdfEngine == QLatin1String("pdflatex"))
            pdf += QStringLiteral(
                "# pdflatex cannot use system fonts or every Unicode character. For those,\n"
                "# install texlive-xetex and use pdf-engine: xelatex.\n");
    }
    pdf += QStringLiteral(
        "# template: ${.}/../templates/Article.tex\n"
        "variables:\n"
        "  geometry: margin=2.5cm\n"
        "  colorlinks: true\n");

    return {
        {QStringLiteral("PDF"), pdf},
        {QStringLiteral("Word"), header + QStringLiteral(
             "\nto: docx\n"
             "# Styles come from a reference document; make one under Templates.\n"
             "# reference-doc: ${.}/../templates/Reference.docx\n")},
        {QStringLiteral("OpenDocument"), header + QStringLiteral(
             "\nto: odt\n"
             "# reference-doc: ${.}/../templates/Reference.odt\n")},
        {QStringLiteral("HTML"), header + QStringLiteral(
             "\nto: html5\n"
             "standalone: true\n"
             "embed-resources: true\n"
             "# template: ${.}/../templates/Page.html\n")},
        {QStringLiteral("LaTeX source"), header + QStringLiteral(
             "\nto: latex\n"
             "standalone: true\n"
             "# template: ${.}/../templates/Article.tex\n")},
    };
}

void Exporter::ensureDefaults() {
    const QString profiles = profilesFolder();
    if (profiles.isEmpty() || QFileInfo(profiles).exists())
        return;
    QDir().mkpath(profiles);
    QDir().mkpath(templatesFolder());
    for (const auto &[name, text] : starterProfiles(detectPdfEngine()))
        writeText(QDir(profiles).filePath(name + QStringLiteral(".yaml")), text);
    reload();
}

QString Exporter::formatOf(const QString &profileText) {
    static const QRegularExpression toRe(QStringLiteral("^(?:to|writer)\\s*:\\s*[\"']?([\\w+\\-]+)"),
                                         QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = toRe.match(profileText);
    // pandoc's own default output is HTML.
    return match.hasMatch() ? match.captured(1).section(QLatin1Char('+'), 0, 0).toLower()
                            : QStringLiteral("html");
}

QString Exporter::extensionFor(const QString &format) {
    static const QHash<QString, QString> extensions{
        {QStringLiteral("pdf"), QStringLiteral("pdf")},
        {QStringLiteral("docx"), QStringLiteral("docx")},
        {QStringLiteral("odt"), QStringLiteral("odt")},
        {QStringLiteral("pptx"), QStringLiteral("pptx")},
        {QStringLiteral("html"), QStringLiteral("html")},
        {QStringLiteral("html5"), QStringLiteral("html")},
        {QStringLiteral("html4"), QStringLiteral("html")},
        {QStringLiteral("latex"), QStringLiteral("tex")},
        {QStringLiteral("beamer"), QStringLiteral("tex")},
        {QStringLiteral("context"), QStringLiteral("tex")},
        {QStringLiteral("typst"), QStringLiteral("typ")},
        {QStringLiteral("epub"), QStringLiteral("epub")},
        {QStringLiteral("epub3"), QStringLiteral("epub")},
        {QStringLiteral("rtf"), QStringLiteral("rtf")},
        {QStringLiteral("plain"), QStringLiteral("txt")},
        {QStringLiteral("markdown"), QStringLiteral("md")},
        {QStringLiteral("gfm"), QStringLiteral("md")},
        {QStringLiteral("commonmark"), QStringLiteral("md")},
        {QStringLiteral("revealjs"), QStringLiteral("html")},
        {QStringLiteral("docbook"), QStringLiteral("xml")},
        {QStringLiteral("jats"), QStringLiteral("xml")},
        {QStringLiteral("asciidoc"), QStringLiteral("adoc")},
        {QStringLiteral("rst"), QStringLiteral("rst")},
        {QStringLiteral("man"), QStringLiteral("1")},
        {QStringLiteral("ms"), QStringLiteral("pdf")},
    };
    return extensions.value(format.toLower(), format.toLower());
}

QVariantList Exporter::profiles() const {
    QVariantList result;
    const QString folder = profilesFolder();
    if (folder.isEmpty())
        return result;
    const QFileInfoList files = QDir(folder).entryInfoList(
        {QStringLiteral("*.yaml"), QStringLiteral("*.yml")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : files) {
        const QString format = formatOf(readText(info.absoluteFilePath()));
        result.append(QVariantMap{{QStringLiteral("name"), info.completeBaseName()},
                                  {QStringLiteral("path"), info.absoluteFilePath()},
                                  {QStringLiteral("format"), format},
                                  {QStringLiteral("extension"), extensionFor(format)}});
    }
    return result;
}

QVariantList Exporter::templates() const {
    QVariantList result;
    const QString folder = templatesFolder();
    if (folder.isEmpty())
        return result;
    const QFileInfoList files = QDir(folder).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : files) {
        const QString suffix = info.suffix().toLower();
        result.append(QVariantMap{
            {QStringLiteral("name"), info.fileName()},
            {QStringLiteral("path"), info.absoluteFilePath()},
            {QStringLiteral("kind"), referenceSuffixes.contains(suffix) ? QStringLiteral("reference")
                                                                         : QStringLiteral("template")},
            {QStringLiteral("text"), textTemplateSuffixes.contains(suffix)}});
    }
    return result;
}

QString Exporter::newTemplate(const QString &kind, const QString &name) {
    m_error.clear();
    if (!pandocAvailable()) {
        m_error = QStringLiteral("pandoc is not installed.");
        return QString();
    }
    const QString folder = templatesFolder();
    if (folder.isEmpty()) {
        m_error = QStringLiteral("Open a notes folder first.");
        return QString();
    }
    QDir().mkpath(folder);

    QString extension;
    QStringList args;
    const QString k = kind.toLower();
    if (k == QLatin1String("latex")) {
        extension = QStringLiteral("tex");
        args = {QStringLiteral("--print-default-template=latex")};
    } else if (k == QLatin1String("html")) {
        extension = QStringLiteral("html");
        args = {QStringLiteral("--print-default-template=html5")};
    } else if (k == QLatin1String("docx") || k == QLatin1String("odt") || k == QLatin1String("pptx")) {
        extension = k;
    } else {
        m_error = QStringLiteral("Unknown kind of template: %1.").arg(kind);
        return QString();
    }

    QString base = Vault::cleanName(name);
    if (base.endsWith(QLatin1Char('.') + extension, Qt::CaseInsensitive))
        base.chop(extension.size() + 1);
    if (base.isEmpty())
        base = extension == QLatin1String("tex") ? QStringLiteral("Article")
             : extension == QLatin1String("html") ? QStringLiteral("Page")
                                                   : QStringLiteral("Reference");
    const QString path = Vault::uniquePath(folder, base, extension);
    if (args.isEmpty())
        args = {QStringLiteral("-o"), path, QStringLiteral("--print-default-data-file"),
                QStringLiteral("reference.") + extension};

    QProcess pandoc;
    pandoc.start(pandocPath(), args);
    if (!pandoc.waitForFinished(30000) || pandoc.exitCode() != 0) {
        m_error = QStringLiteral("pandoc could not make the %1 file: %2")
                      .arg(extension, QString::fromUtf8(pandoc.readAllStandardError()).trimmed());
        QFile::remove(path);
        return QString();
    }
    if (extension == QLatin1String("tex") || extension == QLatin1String("html")) {
        if (!writeText(path, QString::fromUtf8(pandoc.readAllStandardOutput()))) {
            m_error = QStringLiteral("Could not write %1.").arg(QFileInfo(path).fileName());
            return QString();
        }
    }
    reload();
    return path;
}

// Sets key (template or reference-doc) in a profile, replacing an existing
// line for it, commented out or not.
QString Exporter::withTemplate(const QString &profileText, const QString &key, const QString &value) {
    QStringList lines = profileText.split(QLatin1Char('\n'));
    const QRegularExpression keyRe(QStringLiteral("^#?\\s*%1\\s*:").arg(QRegularExpression::escape(key)));
    const QString line = QStringLiteral("%1: %2").arg(key, value);
    for (QString &existing : lines) {
        if (keyRe.match(existing).hasMatch()) {
            existing = line;
            return lines.join(QLatin1Char('\n'));
        }
    }
    QString text = profileText;
    if (!text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');
    return text + line + QLatin1Char('\n');
}

QString Exporter::newProfile(const QString &name, const QString &baseProfile, const QString &templatePath) {
    m_error.clear();
    const QString folder = profilesFolder();
    QString base = Vault::cleanName(name);
    if (folder.isEmpty() || base.isEmpty()) {
        m_error = QStringLiteral("A profile needs a name.");
        return QString();
    }
    QString text = readText(baseProfile);
    if (text.isEmpty())
        text = QStringLiteral("to: pdf\n");
    if (!templatePath.isEmpty()) {
        const QString suffix = QFileInfo(templatePath).suffix().toLower();
        const QString key = referenceSuffixes.contains(suffix) ? QStringLiteral("reference-doc")
                                                               : QStringLiteral("template");
        text = withTemplate(text, key, QStringLiteral("${.}/../templates/") + QFileInfo(templatePath).fileName());
        // A reference document only fits its own format.
        if (key == QLatin1String("reference-doc") && formatOf(text) != suffix) {
            static const QRegularExpression toRe(QStringLiteral("^(to|writer)\\s*:.*$"),
                                                 QRegularExpression::MultilineOption);
            if (toRe.match(text).hasMatch())
                text.replace(toRe, QStringLiteral("to: ") + suffix);
            else
                text.prepend(QStringLiteral("to: ") + suffix + QLatin1Char('\n'));
        }
    }
    QDir().mkpath(folder);
    const QString path = Vault::uniquePath(folder, base, QStringLiteral("yaml"));
    if (!writeText(path, text)) {
        m_error = QStringLiteral("Could not write %1.").arg(QFileInfo(path).fileName());
        return QString();
    }
    reload();
    return path;
}

QString Exporter::defaultOutputPath(const QString &notePath, const QString &profilePath) const {
    const QFileInfo note(notePath);
    const QString extension = extensionFor(formatOf(readText(profilePath)));
    return note.absoluteDir().filePath(note.completeBaseName() + QLatin1Char('.') + extension);
}

// What pandoc reads: [[links]] become their text, and an OmaZettel template
// settings line is dropped.
QString Exporter::prepareText(const QString &text) {
    static const QRegularExpression settingsRe(QStringLiteral("^\\s*<!--\\s*omazettel:.*?-->[ \\t]*\\n?"));
    QString prepared = text;
    prepared.remove(settingsRe);
    return Zettel::plainWikiLinks(prepared);
}

bool Exporter::exportNote(const QString &notePath, const QString &profilePath,
                          const QString &outputPath) {
    return exportText(notePath, profilePath, outputPath, readText(notePath));
}

bool Exporter::exportText(const QString &notePath, const QString &profilePath,
                          const QString &outputPath, const QString &text) {
    m_error.clear();
    if (m_process) {
        m_error = QStringLiteral("An export is already running.");
        return false;
    }
    const QString pandoc = pandocPath();
    if (pandoc.isEmpty()) {
        m_error = QStringLiteral("pandoc is not installed. Install it with: sudo pacman -S pandoc-cli");
        return false;
    }
    if (!QFileInfo(profilePath).isFile()) {
        m_error = QStringLiteral("The export profile is missing.");
        return false;
    }
    const QString &source = text;
    const QFileInfo note(notePath);
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    // No -f: a profile may set its own reader (from: markdown+...), and
    // pandoc reads Markdown from stdin by default.
    QStringList args{QStringLiteral("-d"), profilePath,
                     QStringLiteral("-o"), outputPath,
                     QStringLiteral("--resource-path=%1:%2").arg(note.absolutePath(), m_root.isEmpty() ? note.absolutePath() : m_root),
                     // Only used when the note has no title, so HTML is valid.
                     QStringLiteral("--metadata=pagetitle:%1").arg(note.completeBaseName())};

    m_output = outputPath;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(note.absolutePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const QString messages = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        const bool ok = status == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(m_output);
        QString message = messages;
        if (!ok && message.isEmpty())
            message = status == QProcess::CrashExit ? QStringLiteral("pandoc stopped (took too long or crashed).")
                                                    : QStringLiteral("pandoc failed with exit code %1.").arg(exitCode);
        finish(ok, message);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(false, QStringLiteral("pandoc could not be started."));
    });
    m_process->start(pandoc, args);
    m_process->write(prepareText(source).toUtf8());
    m_process->closeWriteChannel();
    m_timeout.start();
    emit busyChanged();
    return true;
}

void Exporter::cancel() {
    if (m_process)
        m_process->kill();
}

void Exporter::finish(bool ok, const QString &message) {
    if (!m_process)
        return;
    m_timeout.stop();
    m_process->deleteLater();
    m_process = nullptr;
    // Keep only the end of a long LaTeX log.
    QStringList lines = message.split(QLatin1Char('\n'));
    if (lines.size() > 15)
        lines = lines.mid(lines.size() - 15);
    emit busyChanged();
    emit finished(ok, m_output, lines.join(QLatin1Char('\n')));
}
