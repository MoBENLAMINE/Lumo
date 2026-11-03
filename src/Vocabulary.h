#pragma once

#include <QObject>
#include <QVariantList>

// Your words for dictation. Two lists, kept in settings:
//  * words — always spelled your way: "lumo", "LUMO" → "Lumo"; "parakit" → "Parakeet"
//    (same word in any case, or a near miss for longer words)
//  * replacements — say a phrase, get a text: "my email" → "me@example.com"
//  * snippets — say only a short phrase, get a whole text pasted untouched:
//    "sign off" → "Best regards,\nMohammed"
class Vocabulary : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList words READ words NOTIFY changed)
    Q_PROPERTY(QVariantList replacements READ replacements NOTIFY changed)
    Q_PROPERTY(QVariantList snippets READ snippets NOTIFY changed)
public:
    explicit Vocabulary(QObject *parent = nullptr);

    QStringList words() const { return m_words; }
    QVariantList replacements() const;
    QVariantList snippets() const;

    Q_INVOKABLE void addWord(const QString &word);
    Q_INVOKABLE void removeWord(const QString &word);
    Q_INVOKABLE void addReplacement(const QString &spoken, const QString &text);
    Q_INVOKABLE void removeReplacement(const QString &spoken);
    Q_INVOKABLE void addSnippet(const QString &trigger, const QString &text);
    Q_INVOKABLE void removeSnippet(const QString &trigger);

    // The snippet's text when all that was said is its trigger (any case,
    // any punctuation), else an empty string.
    Q_INVOKABLE QString snippet(const QString &heard) const;

    // What a transcript becomes once your words are applied.
    Q_INVOKABLE QString apply(const QString &text) const;

signals:
    void changed();

private:
    void save();

    QStringList m_words;
    QList<QPair<QString, QString>> m_replacements;
    QList<QPair<QString, QString>> m_snippets;
};
