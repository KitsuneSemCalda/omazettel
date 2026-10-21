#pragma once

#include <QString>
#include <QVariantList>

// A line-by-line comparison of two texts, for showing what differs between
// two versions of a note (a sync conflict, or a note's history).
namespace TextDiff {

// [{kind: "same" | "removed" | "added", text}], removed lines being in a
// and not b, added in b and not a.
QVariantList lines(const QString &a, const QString &b);

// How many lines were added and removed.
QPair<int, int> counts(const QVariantList &diff);

}
