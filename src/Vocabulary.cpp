#include "Vocabulary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace {
// Optimal string alignment distance (Damerau–Levenshtein with adjacent swaps).
int distance(const QString &a, const QString &b) {
    const qsizetype n = a.size(), m = b.size();
    QList<QList<int>> d(n + 1, QList<int>(m + 1));
    for (qsizetype i = 0; i <= n; ++i) d[i][0] = int(i);
    for (qsizetype j = 0; j <= m; ++j) d[0][j] = int(j);
    for (qsizetype i = 1; i <= n; ++i)
        for (qsizetype j = 1; j <= m; ++j) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
        }
    return d[n][m];
}

// How far a heard word may be from yours and still be taken for it.
// Short words only match exactly (in any case), or "lump" would become "Lumo".
int tolerance(const QString &word) { return word.size() >= 8 ? 2 : word.size() >= 6 ? 1 : 0; }

QString escapedPhrase(const QString &phrase) {
    // Whole words, any spacing between them.
    QStringList parts;
    for (const auto &w : phrase.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts))
        parts << QRegularExpression::escape(w);
    return QStringLiteral("(?<![\\w])") + parts.join(QStringLiteral("\\s+")) + QStringLiteral("(?![\\w])");
}
}

Vocabulary::Vocabulary(QObject *parent) : QObject(parent) {
    QSettings s;
    m_words = s.value(QStringLiteral("dictation/words")).toStringList();
    for (const auto &v : QJsonDocument::fromJson(s.value(QStringLiteral("dictation/replacements")).toByteArray()).array()) {
        const QJsonObject o = v.toObject();
        m_replacements << qMakePair(o.value(QStringLiteral("spoken")).toString(), o.value(QStringLiteral("text")).toString());
    }
}

QVariantList Vocabulary::replacements() const {
    QVariantList out;
    for (const auto &[spoken, text] : m_replacements)
        out << QVariantMap{{QStringLiteral("spoken"), spoken}, {QStringLiteral("text"), text}};
    return out;
}

void Vocabulary::save() {
    QSettings s;
    s.setValue(QStringLiteral("dictation/words"), m_words);
    QJsonArray list;
    for (const auto &[spoken, text] : m_replacements)
        list.append(QJsonObject{{QStringLiteral("spoken"), spoken}, {QStringLiteral("text"), text}});
    s.setValue(QStringLiteral("dictation/replacements"), QJsonDocument(list).toJson(QJsonDocument::Compact));
    emit changed();
}

void Vocabulary::addWord(const QString &raw) {
    const QString word = raw.simplified();
    if (word.isEmpty() || m_words.contains(word)) return;
    m_words << word;
    m_words.sort(Qt::CaseInsensitive);
    save();
}

void Vocabulary::removeWord(const QString &word) {
    if (m_words.removeAll(word)) save();
}

void Vocabulary::addReplacement(const QString &spokenRaw, const QString &text) {
    const QString spoken = spokenRaw.simplified();
    if (spoken.isEmpty() || text.isEmpty()) return;
    removeReplacement(spoken);
    m_replacements << qMakePair(spoken, text);
    save();
}

void Vocabulary::removeReplacement(const QString &spoken) {
    const auto before = m_replacements.size();
    m_replacements.removeIf([&](const auto &r) { return r.first.compare(spoken, Qt::CaseInsensitive) == 0; });
    if (m_replacements.size() != before) save();
}

QString Vocabulary::apply(const QString &input) const {
    QString text = input;

    // Replacements first, longest phrases first so "my work email" beats "my email".
    auto reps = m_replacements;
    std::sort(reps.begin(), reps.end(), [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
    for (const auto &[spoken, replacement] : reps) {
        const QRegularExpression re(escapedPhrase(spoken), QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        QString out;
        qsizetype last = 0;
        for (auto it = re.globalMatch(text); it.hasNext();) {
            const auto m = it.next();
            out += text.mid(last, m.capturedStart() - last) + replacement;
            last = m.capturedEnd();
        }
        text = out + text.mid(last);
    }

    // Multi-word entries ("Claude Code"): fix their spelling and case as a phrase.
    for (const auto &word : m_words) {
        if (!word.contains(u' ')) continue;
        text.replace(QRegularExpression(escapedPhrase(word), QRegularExpression::CaseInsensitiveOption), word);
    }

    // Single words: same word in another case, or a near miss.
    QList<QString> single;
    for (const auto &w : m_words)
        if (!w.contains(u' ')) single << w;
    if (single.isEmpty()) return text;
    static const QRegularExpression token(QStringLiteral("[\\w'’-]+"), QRegularExpression::UseUnicodePropertiesOption);
    QString out;
    qsizetype last = 0;
    for (auto it = token.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        const QString heard = m.captured(0);
        QString best;
        int bestDistance = 99;
        for (const auto &w : single) {
            if (heard.compare(w, Qt::CaseInsensitive) == 0) {
                best = w;
                bestDistance = 0;
                break;
            }
            const int tol = tolerance(w);
            if (tol == 0 || heard.size() < 3 || std::abs(heard.size() - w.size()) > tol) continue;
            const int dist = distance(heard.toLower(), w.toLower());
            if (dist <= tol && dist < bestDistance) best = w, bestDistance = dist;
        }
        out += text.mid(last, m.capturedStart() - last) + (best.isEmpty() ? heard : best);
        last = m.capturedEnd();
    }
    return out + text.mid(last);
}
