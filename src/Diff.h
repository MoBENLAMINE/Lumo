#pragma once

#include <QString>
#include <QVariantList>

namespace Diff {

struct Result {
    int added = 0;
    int removed = 0;
    // [{t: "+"|"-"|" "|"…", s: line}], context trimmed to 2 lines around changes.
    QVariantList lines;
};

Result compute(const QString &before, const QString &after);

}
