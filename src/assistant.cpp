#include "assistant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

const QString settingKey = QStringLiteral("ai/provider");
// Agents that take the prompt as one argument; Linux caps one argument at 128 KiB.
const int argumentLimit = 120 * 1024;

struct Agent {
    QString id;
    QString name;
    QString binary;
};

const QList<Agent> &agents() {
    static const QList<Agent> list = {
        {QStringLiteral("claude"), QStringLiteral("Claude Code"), QStringLiteral("claude")},
        {QStringLiteral("codex"), QStringLiteral("Codex"), QStringLiteral("codex")},
        {QStringLiteral("gemini"), QStringLiteral("Gemini CLI"), QStringLiteral("gemini")},
        {QStringLiteral("opencode"), QStringLiteral("OpenCode"), QStringLiteral("opencode")},
        {QStringLiteral("cursor-agent"), QStringLiteral("Cursor CLI"), QStringLiteral("cursor-agent")},
        {QStringLiteral("copilot"), QStringLiteral("GitHub Copilot CLI"), QStringLiteral("copilot")},
        {QStringLiteral("crush"), QStringLiteral("Crush"), QStringLiteral("crush")},
        {QStringLiteral("pi"), QStringLiteral("Pi"), QStringLiteral("pi")},
    };
    return list;
}

const Agent *agentNamed(const QString &id) {
    for (const Agent &agent : agents()) {
        if (agent.id == id)
            return &agent;
    }
    return nullptr;
}

QString normalise(QString provider) {
    provider = provider.trimmed().toLower();
    if (provider == QLatin1String("claude-code"))
        return QStringLiteral("claude");
    if (provider == QLatin1String("cursor"))
        return QStringLiteral("cursor-agent");
    if (provider == QLatin1String("gemini-cli"))
        return QStringLiteral("gemini");
    if (provider == QLatin1String("open-code"))
        return QStringLiteral("opencode");
    if (provider == QLatin1String("github-copilot"))
        return QStringLiteral("copilot");
    if (provider == QLatin1String("claude-api"))
        return QStringLiteral("anthropic");
    return provider;
}

QString lastLines(const QString &text, int count = 3) {
    QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.mid(qMax(0, int(lines.size()) - count)).join(QLatin1Char('\n')).trimmed();
}

struct Run {
    int code = -1;
    QByteArray out;
    QString err;
    bool started = false;
};

Run run(const QString &program, const QStringList &arguments, const QByteArray &input,
        const QString &workingDirectory = QString(), int timeoutSeconds = 900) {
    Run result;
    QProcess process;
    if (!workingDirectory.isEmpty())
        process.setWorkingDirectory(workingDirectory);
    process.start(program, arguments);
    if (!process.waitForStarted(15000))
        return result;
    result.started = true;
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutSeconds * 1000)) {
        process.kill();
        process.waitForFinished(3000);
        result.err = QStringLiteral("timed out after %1 minutes").arg(timeoutSeconds / 60);
        return result;
    }
    result.out = process.readAllStandardOutput();
    result.err = QString::fromUtf8(process.readAllStandardError());
    result.code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    return result;
}

QString ollamaUrl() {
    QString host = qEnvironmentVariable("OLLAMA_HOST", QStringLiteral("http://localhost:11434"));
    if (!host.startsWith(QLatin1String("http")))
        host.prepend(QStringLiteral("http://"));
    while (host.endsWith(QLatin1Char('/')))
        host.chop(1);
    return host;
}

QString ollamaModel(const QString &model) {
    return model.isEmpty() ? QStringLiteral("llama3.2") : model;
}

// curl with headers (which may hold a key) on standard input, out of the
// process list, and the body from a file.
Run curlJson(const QString &url, const QStringList &headers, const QByteArray &body, int timeoutSeconds) {
    const QString curl = QStandardPaths::findExecutable(QStringLiteral("curl"));
    Run result;
    if (curl.isEmpty()) {
        result.err = QStringLiteral("curl is not installed");
        return result;
    }
    QTemporaryDir work;
    const QString bodyPath = work.filePath(QStringLiteral("body.json"));
    QFile file(bodyPath);
    if (!file.open(QIODevice::WriteOnly)) {
        result.err = QStringLiteral("could not write a temporary file");
        return result;
    }
    file.write(body);
    file.close();
    QByteArray config;
    for (const QString &header : headers)
        config += "header = \"" + header.toUtf8() + "\"\n";
    QStringList arguments{QStringLiteral("-sS"), QStringLiteral("--max-time"), QString::number(timeoutSeconds),
                          QStringLiteral("-K"), QStringLiteral("-"), QStringLiteral("-H"),
                          QStringLiteral("Content-Type: application/json")};
    if (!body.isEmpty())
        arguments << QStringLiteral("--data-binary") << QLatin1Char('@') + bodyPath;
    arguments << url;
    return run(curl, arguments, config.isEmpty() ? QByteArray("\n") : config, QString(), timeoutSeconds + 30);
}

QString apiError(const Run &result, const QJsonObject &answer) {
    if (!result.started || result.code != 0)
        return result.err.trimmed().isEmpty() ? QStringLiteral("the request failed") : lastLines(result.err);
    const QJsonValue error = answer.value(QStringLiteral("error"));
    if (error.isObject())
        return error.toObject().value(QStringLiteral("message")).toString();
    if (error.isString())
        return error.toString();
    return QString();
}

// An agent's error output, cut to what matters: no hook chatter, the
// message out of JSON error objects, each said once.
QString agentErrors(const QString &errors) {
    static const QRegularExpression messageRe(QStringLiteral("\"message\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\""));
    QStringList kept;
    QStringList errorLines;
    for (const QString &raw : errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("hook:")) || line.startsWith(QLatin1String("warning:"))
                || line == QLatin1String(">>>") || line == QLatin1String("<<<") || line.isEmpty())
            continue;
        const QRegularExpressionMatch message = messageRe.match(line);
        static const QRegularExpression errorRe(QStringLiteral("^(ERROR|Error|error|fatal|Fatal):\\s*"));
        const bool isError = message.hasMatch() || errorRe.match(line).hasMatch();
        if (message.hasMatch())
            line = message.captured(1).replace(QLatin1String("\\\""), QLatin1String("\""));
        line.remove(errorRe);
        QStringList &into = isError ? errorLines : kept;
        if (!into.contains(line))
            into.append(line);
    }
    // Agents may echo the prompt; their own error lines say what went wrong.
    const QStringList &said = errorLines.isEmpty() ? kept : errorLines;
    return said.mid(qMax(0, int(said.size()) - 3)).join(QLatin1Char('\n'));
}

QString modelHint(const Assistant::Choice &choice, const QString &errors) {
    if (choice.model.isEmpty() || !errors.contains(QLatin1String("model"), Qt::CaseInsensitive))
        return QString();
    return QStringLiteral("\nThe model “%1” is not available to your %2 account. Leave it out (--ai %3) to use the "
                          "agent's default, or name one it offers.").arg(choice.model, Assistant::displayName(choice.provider), choice.provider);
}

QString signInHint(const QString &provider, const QString &errors) {
    static const QRegularExpression authRe(QStringLiteral("log ?in|auth|api key|credential|unauthori[sz]ed|not signed"),
                                           QRegularExpression::CaseInsensitiveOption);
    if (!authRe.match(errors).hasMatch())
        return QString();
    const Agent *agent = agentNamed(provider);
    return agent ? QStringLiteral("\nSign in to %1 first: run `%2` once in a terminal.").arg(agent->name, agent->binary)
                 : QString();
}

}

QStringList Assistant::supported() {
    QStringList ids;
    for (const Agent &agent : agents())
        ids.append(agent.id);
    ids << QStringLiteral("ollama") << QStringLiteral("anthropic") << QStringLiteral("openai");
    return ids;
}

QString Assistant::displayName(const QString &provider) {
    if (const Agent *agent = agentNamed(provider))
        return agent->name;
    if (provider == QLatin1String("ollama"))
        return QStringLiteral("Ollama");
    if (provider == QLatin1String("anthropic"))
        return QStringLiteral("the Claude API");
    if (provider == QLatin1String("openai"))
        return QStringLiteral("the OpenAI API");
    return provider;
}

QString Assistant::saved() {
    return QSettings().value(settingKey).toString();
}

void Assistant::save(const QString &choice) {
    if (choice.trimmed().isEmpty())
        QSettings().remove(settingKey);
    else
        QSettings().setValue(settingKey, choice.trimmed());
}

QString Assistant::omarchyDefaultAgent() {
    QFile file(QDir::homePath() + QStringLiteral("/.config/omarchy/defaults/agent"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readLine()).trimmed();
}

QStringList Assistant::installedAgents() {
    QStringList found;
    for (const Agent &agent : agents()) {
        if (!QStandardPaths::findExecutable(agent.binary).isEmpty())
            found.append(agent.id);
    }
    return found;
}

Assistant::Choice Assistant::choose(const QString &requested) {
    QString text;
    QString origin;
    if (!requested.trimmed().isEmpty()) {
        text = requested;
        origin = QStringLiteral("--ai");
    } else if (!qEnvironmentVariable("OMAZETTEL_AI").isEmpty()) {
        text = qEnvironmentVariable("OMAZETTEL_AI");
        origin = QStringLiteral("$OMAZETTEL_AI");
    } else if (!saved().isEmpty()) {
        text = saved();
        origin = QStringLiteral("saved with omazettel ai --set");
    } else if (!omarchyDefaultAgent().isEmpty()) {
        text = omarchyDefaultAgent();
        origin = QStringLiteral("Omarchy's default agent");
    }
    Choice choice;
    choice.origin = origin;
    const int colon = int(text.indexOf(QLatin1Char(':')));
    choice.provider = normalise(colon < 0 ? text : text.left(colon));
    choice.model = colon < 0 ? QString() : text.mid(colon + 1).trimmed();
    return choice;
}

QString Assistant::problem(const Choice &choice) {
    const QStringList installed = installedAgents();
    const QString options = QStringLiteral(
        "  --ai claude | codex | gemini | opencode | cursor-agent | copilot | crush | pi   (an agent you have)\n"
        "  --ai ollama:<model>          a local model through Ollama\n"
        "  --ai anthropic[:<model>]     the Claude API (ANTHROPIC_API_KEY)\n"
        "  --ai openai:<model>          an OpenAI-compatible API (OPENAI_API_KEY)\n"
        "Save one with: omazettel ai --set <choice>");
    if (choice.provider.isEmpty() || choice.provider == QLatin1String("none")) {
        QString text = QStringLiteral("No AI is chosen to write the note.\n");
        if (!installed.isEmpty())
            text += QStringLiteral("Installed here: %1. ").arg(installed.join(QStringLiteral(", ")));
        if (QFileInfo::exists(QStringLiteral("/usr/share/omarchy")))
            text += QStringLiteral("On Omarchy, pick a default agent with: omarchy default agent <name>\nOr:\n");
        return text + options;
    }
    if (!supported().contains(choice.provider)) {
        return QStringLiteral("OmaZettel cannot write notes with %1 (%2). Use one of these instead%3:\n%4")
            .arg(choice.provider, choice.origin,
                 installed.isEmpty() ? QString() : QStringLiteral(" (installed: %1)").arg(installed.join(QStringLiteral(", "))),
                 options);
    }
    if (const Agent *agent = agentNamed(choice.provider)) {
        if (QStandardPaths::findExecutable(agent->binary).isEmpty())
            return QStringLiteral("%1 is not installed. Install it with: omarchy default agent %2\n(or choose another: %3)")
                .arg(agent->name, agent->id, installed.isEmpty() ? options : installed.join(QStringLiteral(", ")));
        return QString();
    }
    if (choice.provider == QLatin1String("ollama")) {
        if (QStandardPaths::findExecutable(QStringLiteral("curl")).isEmpty())
            return QStringLiteral("Talking to Ollama needs curl.");
        const Run tags = curlJson(ollamaUrl() + QStringLiteral("/api/tags"), {}, QByteArray(), 5);
        if (tags.code != 0) {
            return QStringLiteral("Ollama is not running at %1.\n%2Start it with: ollama serve (or systemctl start ollama)")
                .arg(ollamaUrl(), QStandardPaths::findExecutable(QStringLiteral("ollama")).isEmpty()
                                      ? QStringLiteral("Install it with: omarchy pkg add ollama\n") : QString());
        }
        const QString wanted = ollamaModel(choice.model);
        bool have = false;
        for (const QJsonValue &model : QJsonDocument::fromJson(tags.out).object().value(QStringLiteral("models")).toArray()) {
            const QString name = model.toObject().value(QStringLiteral("name")).toString();
            if (name == wanted || name == wanted + QStringLiteral(":latest"))
                have = true;
        }
        if (!have)
            return QStringLiteral("Ollama does not have the model %1. Download it with: ollama pull %1").arg(wanted);
        return QString();
    }
    if (choice.provider == QLatin1String("anthropic")) {
        if (qEnvironmentVariable("ANTHROPIC_API_KEY").isEmpty())
            return QStringLiteral("The Claude API needs an API key: export ANTHROPIC_API_KEY=... "
                                  "(from console.anthropic.com).%1")
                .arg(installed.contains(QStringLiteral("claude")) ? QStringLiteral(" Or use your Claude Code sign-in: --ai claude") : QString());
        return QString();
    }
    if (choice.provider == QLatin1String("openai")) {
        if (choice.model.isEmpty())
            return QStringLiteral("Name the model: --ai openai:<model>");
        if (qEnvironmentVariable("OPENAI_API_KEY").isEmpty() && qEnvironmentVariable("OPENAI_BASE_URL").isEmpty())
            return QStringLiteral("The OpenAI API needs an API key: export OPENAI_API_KEY=... "
                                  "(or OPENAI_BASE_URL for another OpenAI-compatible server).");
        return QString();
    }
    return QString();
}

QString Assistant::complete(const Choice &choice, const QString &prompt, QString *error) {
    const QString trouble = problem(choice);
    if (!trouble.isEmpty()) {
        *error = trouble;
        return QString();
    }
    const QByteArray input = prompt.toUtf8();

    if (const Agent *agent = agentNamed(choice.provider)) {
        // A scratch folder, so the agent sees no project around it.
        QTemporaryDir work;
        const QString program = QStandardPaths::findExecutable(agent->binary);
        QStringList arguments;
        QByteArray standardInput;
        QString outputFile;
        bool asArgument = false;
        if (agent->id == QLatin1String("claude")) {
            arguments << QStringLiteral("-p") << QStringLiteral("--output-format") << QStringLiteral("text")
                      << QStringLiteral("--tools") << QString() << QStringLiteral("--strict-mcp-config")
                      << QStringLiteral("--no-session-persistence");
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("--model") << choice.model;
            standardInput = input;
        } else if (agent->id == QLatin1String("codex")) {
            outputFile = work.filePath(QStringLiteral("answer.txt"));
            arguments << QStringLiteral("exec") << QStringLiteral("--skip-git-repo-check") << QStringLiteral("--ephemeral")
                      << QStringLiteral("--sandbox") << QStringLiteral("read-only") << QStringLiteral("-o") << outputFile;
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("-m") << choice.model;
            arguments << QStringLiteral("-");
            standardInput = input;
        } else if (agent->id == QLatin1String("gemini")) {
            arguments << QStringLiteral("-p") << QStringLiteral("Follow the instructions above exactly.")
                      << QStringLiteral("-o") << QStringLiteral("text") << QStringLiteral("--approval-mode") << QStringLiteral("plan");
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("-m") << choice.model;
            standardInput = input;
        } else if (agent->id == QLatin1String("opencode")) {
            const QString promptFile = work.filePath(QStringLiteral("instructions.md"));
            QFile file(promptFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(input);
                file.close();
            }
            arguments << QStringLiteral("run") << QStringLiteral("--file") << promptFile;
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("-m") << choice.model;
            arguments << QStringLiteral("Follow the instructions in the attached file exactly.");
        } else if (agent->id == QLatin1String("cursor-agent")) {
            arguments << QStringLiteral("-p") << QStringLiteral("--output-format") << QStringLiteral("text") << QStringLiteral("--trust");
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("--model") << choice.model;
            arguments << prompt;
            asArgument = true;
        } else if (agent->id == QLatin1String("copilot")) {
            arguments << QStringLiteral("-p") << prompt;
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("--model") << choice.model;
            asArgument = true;
        } else if (agent->id == QLatin1String("crush")) {
            arguments << QStringLiteral("run") << prompt;
            asArgument = true;
        } else if (agent->id == QLatin1String("pi")) {
            arguments << QStringLiteral("-p") << prompt;
            if (!choice.model.isEmpty())
                arguments << QStringLiteral("--model") << choice.model;
            asArgument = true;
        }
        if (asArgument && input.size() > argumentLimit) {
            *error = QStringLiteral("This transcript is too long to hand to %1 on its command line. "
                                    "Use --ai claude, codex, gemini, opencode or ollama:<model> for it.").arg(agent->name);
            return QString();
        }
        const Run result = run(program, arguments, standardInput, work.path());
        if (!result.started || result.code != 0) {
            const QString said = agentErrors(result.err + QLatin1Char('\n') + QString::fromUtf8(result.out));
            *error = QStringLiteral("%1 could not write the note: %2%3%4")
                         .arg(agent->name, said.isEmpty() ? QStringLiteral("it stopped with an error") : said,
                              modelHint(choice, said), signInHint(agent->id, said));
            return QString();
        }
        if (!outputFile.isEmpty()) {
            QFile file(outputFile);
            if (file.open(QIODevice::ReadOnly))
                return QString::fromUtf8(file.readAll());
        }
        return QString::fromUtf8(result.out);
    }

    if (choice.provider == QLatin1String("ollama")) {
        const QJsonObject body{{QStringLiteral("model"), ollamaModel(choice.model)},
                               {QStringLiteral("prompt"), prompt},
                               {QStringLiteral("stream"), false},
                               {QStringLiteral("format"), QStringLiteral("json")},
                               {QStringLiteral("options"), QJsonObject{{QStringLiteral("num_ctx"), 32768}}}};
        const Run result = curlJson(ollamaUrl() + QStringLiteral("/api/generate"), {},
                                    QJsonDocument(body).toJson(QJsonDocument::Compact), 1800);
        const QJsonObject answer = QJsonDocument::fromJson(result.out).object();
        const QString failure = apiError(result, answer);
        if (!failure.isEmpty()) {
            *error = QStringLiteral("Ollama could not write the note: %1").arg(failure);
            return QString();
        }
        return answer.value(QStringLiteral("response")).toString();
    }

    if (choice.provider == QLatin1String("anthropic")) {
        const QJsonObject body{{QStringLiteral("model"), choice.model.isEmpty() ? QStringLiteral("claude-opus-5-5") : choice.model},
                               {QStringLiteral("max_tokens"), 16000},
                               {QStringLiteral("fallbacks"), QStringLiteral("default")},
                               {QStringLiteral("messages"), QJsonArray{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                                                                   {QStringLiteral("content"), prompt}}}}};
        const Run result = curlJson(QStringLiteral("https://api.anthropic.com/v1/messages"),
                                    {QStringLiteral("x-api-key: ") + qEnvironmentVariable("ANTHROPIC_API_KEY"),
                                     QStringLiteral("anthropic-version: 2023-06-01"),
                                     QStringLiteral("anthropic-beta: server-side-fallback-2026-07-01")},
                                    QJsonDocument(body).toJson(QJsonDocument::Compact), 600);
        const QJsonObject answer = QJsonDocument::fromJson(result.out).object();
        const QString failure = apiError(result, answer);
        if (!failure.isEmpty()) {
            *error = QStringLiteral("The Claude API could not write the note: %1").arg(failure);
            return QString();
        }
        if (answer.value(QStringLiteral("stop_reason")).toString() == QLatin1String("refusal")) {
            *error = QStringLiteral("The Claude API declined to summarise this transcript.");
            return QString();
        }
        QString text;
        for (const QJsonValue &block : answer.value(QStringLiteral("content")).toArray()) {
            if (block.toObject().value(QStringLiteral("type")).toString() == QLatin1String("text"))
                text += block.toObject().value(QStringLiteral("text")).toString();
        }
        return text;
    }

    if (choice.provider == QLatin1String("openai")) {
        QString base = qEnvironmentVariable("OPENAI_BASE_URL", QStringLiteral("https://api.openai.com/v1"));
        while (base.endsWith(QLatin1Char('/')))
            base.chop(1);
        const QJsonObject body{{QStringLiteral("model"), choice.model},
                               {QStringLiteral("messages"), QJsonArray{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                                                                   {QStringLiteral("content"), prompt}}}},
                               {QStringLiteral("response_format"), QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}}}};
        QStringList headers;
        if (!qEnvironmentVariable("OPENAI_API_KEY").isEmpty())
            headers << QStringLiteral("Authorization: Bearer ") + qEnvironmentVariable("OPENAI_API_KEY");
        const Run result = curlJson(base + QStringLiteral("/chat/completions"), headers,
                                    QJsonDocument(body).toJson(QJsonDocument::Compact), 600);
        const QJsonObject answer = QJsonDocument::fromJson(result.out).object();
        const QString failure = apiError(result, answer);
        if (!failure.isEmpty()) {
            *error = QStringLiteral("The OpenAI API could not write the note: %1").arg(failure);
            return QString();
        }
        return answer.value(QStringLiteral("choices")).toArray().first().toObject()
            .value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
    }
    *error = QStringLiteral("Unknown AI %1.").arg(choice.provider);
    return QString();
}

QJsonObject Assistant::extractJson(const QString &text, QString *error) {
    const int start = int(text.indexOf(QLatin1Char('{')));
    const int end = int(text.lastIndexOf(QLatin1Char('}')));
    if (start < 0 || end <= start) {
        *error = QStringLiteral("The AI did not answer with a note. It said: %1").arg(text.trimmed().left(300));
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8(), &parseError);
    if (!document.isObject()) {
        *error = QStringLiteral("The AI's answer was not valid JSON (%1).").arg(parseError.errorString());
        return {};
    }
    return document.object();
}
