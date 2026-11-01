#pragma once

#include <QObject>
#include <QVariantList>

// Your words for dictation. Two lists, kept in settings:
//  * words — always spelled your way: "lumo", "LUMO" → "Lumo"; "parakit" → "Parakeet"
//    (same word in any case, or a near miss for longer words)
//  * replacements — say a phrase, get a text: "my email" → "me@example.com"
class Vocabulary : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList words READ words NOTIFY changed)
    Q_PROPERTY(QVariantList replacements READ replacements NOTIFY changed)
public:
    explicit Vocabulary(QObject *parent = nullptr);

    QStringList words() const { return m_words; }
    QVariantList replacements() const;

    Q_INVOKABLE void addWord(const QString &word);
    Q_INVOKABLE void removeWord(const QString &word);
    Q_INVOKABLE void addReplacement(const QString &spoken, const QString &text);
    Q_INVOKABLE void removeReplacement(const QString &spoken);

    // What a transcript becomes once your words are applied.
    Q_INVOKABLE QString apply(const QString &text) const;

signals:
    void changed();

private:
    void save();

    QStringList m_words;
    QList<QPair<QString, QString>> m_replacements;
};
