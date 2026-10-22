#pragma once

#include <QJsonObject>
#include <QString>

class NotesApi;

// `omazettel mcp`: the notes tools for AI assistants (Claude Code, Claude
// Desktop, Cursor, ...) over the Model Context Protocol, as JSON-RPC lines
// on standard input and output.
namespace McpServer {

// Answers one message; an empty object means no answer (a notification).
QJsonObject handle(NotesApi &api, const QJsonObject &message, const QString &version);

// Reads messages until standard input closes. Returns the exit code.
int run(NotesApi &api, const QString &version);

}
