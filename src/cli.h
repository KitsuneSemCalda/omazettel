#pragma once

#include <QString>
#include <QStringList>
#include <optional>

// Command line handling that runs before the GUI starts: `omazettel --help`,
// `--version`, the note verbs (`omazettel search`, `new`, `append`, ...) and
// `omazettel mcp` answer in the terminal instead of opening a window.
namespace Cli {

QString usage();
// Help for one verb, or "" when there is no such verb.
QString verbUsage(const QString &verb);

// Whether the arguments name a verb (so they need no window).
bool isCommand(const QStringList &arguments);

// The exit code main should return when the arguments are answered without
// starting the app, or nothing when OmaZettel should open as usual. Anything
// else, Qt's own options included, is left for the app. Verbs need a
// QCoreApplication to exist.
std::optional<int> handleArguments(const QStringList &arguments, const QString &version);
std::optional<int> handleArguments(int argc, char *argv[], const QString &version);

}
