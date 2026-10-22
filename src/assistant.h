#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// The AI that writes notes from transcripts. OmaZettel does not ship a model:
// it asks one the person already has.
//
//   claude, codex, gemini, opencode, cursor-agent, copilot, crush, pi
//       A coding agent's command line, run once with no tools, as text in
//       and text out. On Omarchy, the default agent (omarchy default agent).
//   ollama[:model]          A local model through Ollama.
//   anthropic[:model]       The Claude API, with ANTHROPIC_API_KEY.
//   openai:model            An OpenAI-compatible API, with OPENAI_API_KEY
//                           (and OPENAI_BASE_URL for other servers).
//
// "provider:model" picks a model, e.g. claude:sonnet, ollama:llama3.2.
// Which one: --ai, else $OMAZETTEL_AI, else the saved choice
// (omazettel ai --set ...), else Omarchy's default agent.
namespace Assistant {

struct Choice {
    QString provider; // Empty when nothing is chosen.
    QString model;    // Empty for the provider's own default.
    QString origin;   // Where the choice came from, for messages.
};

QStringList supported();
QString displayName(const QString &provider);
Choice choose(const QString &requested);
// The saved choice (QSettings ai/provider); "" clears it.
QString saved();
void save(const QString &choice);
// Omarchy's default agent, or "".
QString omarchyDefaultAgent();
// The supported agents installed here.
QStringList installedAgents();

// What stops the choice from working, with how to fix it, or "".
QString problem(const Choice &choice);
// Runs the prompt. Returns the model's text, or "" with error set.
QString complete(const Choice &choice, const QString &prompt, QString *error);
// The JSON object in a model's answer (code fences and chatter around it
// are allowed), or {} with error set.
QJsonObject extractJson(const QString &text, QString *error);

}
