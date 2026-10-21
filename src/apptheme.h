#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QVariantMap>

// OmaZettel's colours, named the way its theme file names them:
// "editor.background", "editor.link", "ui.panel", ... (see
// themed/omazettel.toml.tpl). They come from, in order:
//   1. omazettel.toml in the current Omarchy theme (shipped by the theme, or
//      rendered by Omarchy from a template when the theme was set);
//   2. ~/.config/omarchy/themed/omazettel.toml.tpl, rendered here so edits
//      show at once;
//   3. the built-in template, rendered here from the theme's colours.
// or the fixed Oma palette. Missing names are filled from the core colours.
namespace AppTheme {

struct Result {
    QHash<QString, QString> colors; // name -> "#rrggbb"
    QString source;                 // "theme file", "your template", "built-in template", "Oma palette"
    QString path;                   // The file it came from, if any.
};

QString themeDir();
QString userTemplatePath();
QString builtinTemplate();

// The theme's named colours (colors.toml, with Omarchy's aliases and derived
// shades when omarchy-theme-color is there).
QHash<QString, QString> themeColors(const QString &colorsFile);

// Fills {{ name }}, {{ name_strip }}, {{ name_rgb }} and {{ mix a b 30% }}.
QString render(const QString &templateText, const QHash<QString, QString> &colors);

// [section] / key = "value" pairs as "section.key" -> value, valid colours only.
QHash<QString, QString> parse(const QString &toml);

Result load();
Result omaPalette(const QString &accent);

// Fills every name the app uses that is missing, from the core colours.
void complete(QHash<QString, QString> &colors, bool dark);

QStringList names();
}
