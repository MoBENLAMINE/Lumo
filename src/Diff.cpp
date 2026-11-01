#include "Diff.h"

#include <QStringList>
#include <QVariantMap>
#include <vector>

namespace Diff {

namespace {
constexpr qsizetype kMaxCells = 4'000'000;
constexpr int kContext = 2;
constexpr int kMaxShown = 400;

QVariantMap line(const char *t, const QString &s) {
    return QVariantMap{{QStringLiteral("t"), QString::fromLatin1(t)}, {QStringLiteral("s"), s}};
}
}

Result compute(const QString &before, const QString &after) {
    const QStringList a = before.isEmpty() ? QStringList() : before.split(u'\n');
    const QStringList b = after.isEmpty() ? QStringList() : after.split(u'\n');
    const qsizetype n = a.size(), m = b.size();

    // Raw edit script: ' ' kept, '-' removed, '+' added.
    std::vector<std::pair<char, QString>> ops;
    if ((n + 1) * (m + 1) > kMaxCells) {
        for (const auto &s : a) ops.emplace_back('-', s);
        for (const auto &s : b) ops.emplace_back('+', s);
    } else {
        // LCS table from the end so the walk below goes forwards.
        std::vector<uint32_t> t((n + 1) * (m + 1), 0);
        auto at = [&](qsizetype i, qsizetype j) -> uint32_t & { return t[i * (m + 1) + j]; };
        for (qsizetype i = n - 1; i >= 0; --i)
            for (qsizetype j = m - 1; j >= 0; --j)
                at(i, j) = a[i] == b[j] ? at(i + 1, j + 1) + 1 : std::max(at(i + 1, j), at(i, j + 1));
        qsizetype i = 0, j = 0;
        while (i < n && j < m) {
            if (a[i] == b[j]) ops.emplace_back(' ', a[i++]), ++j;
            else if (at(i + 1, j) >= at(i, j + 1)) ops.emplace_back('-', a[i++]);
            else ops.emplace_back('+', b[j++]);
        }
        while (i < n) ops.emplace_back('-', a[i++]);
        while (j < m) ops.emplace_back('+', b[j++]);
    }

    Result r;
    std::vector<bool> keep(ops.size(), false);
    for (size_t k = 0; k < ops.size(); ++k) {
        if (ops[k].first == ' ') continue;
        (ops[k].first == '+' ? r.added : r.removed)++;
        for (size_t c = k >= kContext ? k - kContext : 0; c <= std::min(ops.size() - 1, k + kContext); ++c)
            keep[c] = true;
    }
    bool gap = false;
    for (size_t k = 0; k < ops.size() && r.lines.size() < kMaxShown; ++k) {
        if (!keep[k]) {
            gap = true;
            continue;
        }
        if (gap && !r.lines.isEmpty()) r.lines.append(line("…", QString()));
        gap = false;
        const char t[2] = {ops[k].first, 0};
        r.lines.append(line(t, ops[k].second));
    }
    return r;
}

}
