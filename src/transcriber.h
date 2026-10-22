#pragma once

#include <QString>
#include <QStringList>

// Turning a meeting into text a note can be written from: caption files
// (Teams, Zoom and Meet .vtt, .srt), Word transcripts (.docx, through
// pandoc), plain text, or a recording (audio or video, through ffmpeg and
// whisper.cpp, or OpenAI's whisper if that is what is installed).
namespace Transcriber {

struct Result {
    QString text;
    QString engine;       // captions, pandoc, text, whisper.cpp, whisper.
    QStringList speakers; // Named in the captions, in order of first word.
    QString model;        // The whisper model used, for recordings.
};

// Caption cues as "Speaker: words" paragraphs, one per turn, with the turn's
// start time in front when timestamps is set.
QString fromCaptions(const QString &captions, bool timestamps, QStringList *speakers = nullptr);

// The speech-to-text program found: voxtype (Omarchy's dictation, with its
// model), whisper.cpp or whisper; or "".
QString engine();
// What the engine needs before it can transcribe a recording, with how to
// get it, or "".
QString problem(const QString &model = QString());
// The model the engine will use, for messages.
QString engineModel(const QString &model = QString());
// Where downloaded whisper.cpp models go: ~/.local/share/omazettel/whisper.
QString modelsFolder();
// A model file: wanted as a path or a name (base.en), else
// $OMAZETTEL_WHISPER_MODEL, else the best one already downloaded.
QString findModel(const QString &wanted);
// Downloads a whisper.cpp model (tiny, base.en, small, large-v3-turbo, ...).
bool downloadModel(const QString &name, QString *path, QString *error);

// file may also be voxtype:latest or voxtype:<id>, a meeting recorded with
// `voxtype meeting start`, which comes with speaker labels.
// Throws nothing; error is set and the result empty on failure.
Result transcribe(const QString &file, const QString &model, const QString &language,
                  bool timestamps, QString *error);

}
