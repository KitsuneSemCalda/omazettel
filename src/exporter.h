#pragma once

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Exports notes with pandoc, the way Zettlr does with its custom templates
// and reference documents, but using pandoc's own files so the same export
// also works from a terminal:
//
//   <notes>/.omazettel/export/*.yaml    Profiles: pandoc defaults files
//                                        (to, pdf-engine, template,
//                                        reference-doc, variables, ...).
//   <notes>/.omazettel/templates/       Templates (.tex, .html, ...) and
//                                        reference documents (.docx, .odt,
//                                        .pptx) that profiles point to with
//                                        ${.}/../templates/<file>.
//
//   pandoc -d .omazettel/export/PDF.yaml note.md -o note.pdf
class Exporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool pandocAvailable READ pandocAvailable CONSTANT)
    Q_PROPERTY(QString profilesFolder READ profilesFolder NOTIFY changed)
    Q_PROPERTY(QString templatesFolder READ templatesFolder NOTIFY changed)
    Q_PROPERTY(QString lastProfile READ lastProfile WRITE setLastProfile NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit Exporter(QObject *parent = nullptr);
    ~Exporter() override;

    bool busy() const { return m_process != nullptr; }
    bool pandocAvailable() const;
    QString profilesFolder() const;
    QString templatesFolder() const;
    QString lastProfile() const;
    void setLastProfile(const QString &path);
    int revision() const { return m_revision; }

    Q_INVOKABLE void setRoot(const QString &notesRoot);
    // Writes the starter profiles if the export folder does not exist.
    Q_INVOKABLE void ensureDefaults();
    Q_INVOKABLE void reload() { ++m_revision; emit changed(); }

    // [{name, path, format, extension}], by name.
    Q_INVOKABLE QVariantList profiles() const;
    // [{name, path, kind ("template" or "reference"), text (opens in the
    // editor)}], by name.
    Q_INVOKABLE QVariantList templates() const;

    // A new template or reference document from pandoc's default. kind is
    // latex, html, docx, odt or pptx. Returns the path, or "" (see error()).
    Q_INVOKABLE QString newTemplate(const QString &kind, const QString &name);
    // A new profile copied from baseProfile, using templatePath (a template
    // or reference document in the templates folder) if given.
    Q_INVOKABLE QString newProfile(const QString &name, const QString &baseProfile,
                                   const QString &templatePath = QString());
    Q_INVOKABLE QString defaultOutputPath(const QString &notePath, const QString &profilePath) const;
    Q_INVOKABLE QString error() const { return m_error; }

    // Starts exporting the note's file. Emits finished().
    Q_INVOKABLE bool exportNote(const QString &notePath, const QString &profilePath,
                                const QString &outputPath);
    // The same, with text in place of the file (the note open in the editor,
    // unsaved changes included).
    Q_INVOKABLE bool exportText(const QString &notePath, const QString &profilePath,
                                const QString &outputPath, const QString &text);
    Q_INVOKABLE void cancel();

    // Helpers, public for the tests.
    static QString formatOf(const QString &profileText);
    static QString extensionFor(const QString &format);
    static QString detectPdfEngine();
    static QList<QPair<QString, QString>> starterProfiles(const QString &pdfEngine);
    static QString withTemplate(const QString &profileText, const QString &key,
                                const QString &value);
    static QString prepareText(const QString &text);

signals:
    void changed();
    void busyChanged();
    // ok, the output path, and pandoc's messages (warnings or the error).
    void finished(bool ok, const QString &outputPath, const QString &message);

private:
    void finish(bool ok, const QString &message);

    QString m_root;
    QString m_error;
    int m_revision = 0;
    QPointer<QProcess> m_process;
    QString m_output;
    QTimer m_timeout;
};
