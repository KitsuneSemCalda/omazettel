#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

// Citations as pandoc writes them ([@key], [@key, p. 4; @other]), from a
// bibliography in BibTeX (.bib) or CSL JSON (.json), the formats Zotero's
// Better BibTeX keeps exported. Settings are in <notes>/.omazettel/citations.yaml:
//   bibliography: References.bib      (relative to the notes folder, or absolute)
//   csl: styles/apa.csl               (optional citation style)
// Export passes them to pandoc's --citeproc; the editor completes keys after [@.
class Citations : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString bibliography READ bibliography NOTIFY changed)
    Q_PROPERTY(QString style READ style NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    struct Settings {
        QString bibliography; // Absolute, or empty.
        QString style;        // Absolute, or empty.
    };

    struct Entry {
        QString key;
        QString author; // First author's family name, "et al." for more.
        QString year;
        QString title;
    };

    explicit Citations(QObject *parent = nullptr);

    QString bibliography() const { return m_settings.bibliography; }
    QString style() const { return m_settings.style; }
    int count() const { return int(m_entries.size()); }
    QString error() const { return m_error; }
    int revision() const { return m_revision; }

    Q_INVOKABLE void setRoot(const QString &notesRoot);
    Q_INVOKABLE bool setBibliography(const QString &path);
    Q_INVOKABLE bool setStyle(const QString &path);
    Q_INVOKABLE void reload();

    // For the completer: [{name (key), title ("Author Year · Title"), folder: ""}].
    Q_INVOKABLE QVariantList entries() const;
    Q_INVOKABLE QString describe(const QString &key) const;

    static Settings settingsFor(const QString &notesRoot);
    static QList<Entry> parseBibTeX(const QString &text);
    static QList<Entry> parseCslJson(const QByteArray &json);

signals:
    void changed();

private:
    void save();
    void watch();

    QString m_root;
    Settings m_settings;
    QList<Entry> m_entries;
    QString m_error;
    int m_revision = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
