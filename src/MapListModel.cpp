#include "MapListModel.h"

MapListModel::MapListModel(const QStringList &roles, const QString &keyRole, QObject *parent)
    : QAbstractListModel(parent), m_roles(roles), m_key(keyRole) {}

int MapListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant MapListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) return {};
    const int r = role - Qt::UserRole - 1;
    if (r < 0 || r >= m_roles.size()) return {};
    return m_rows[index.row()].value(m_roles[r]);
}

QHash<int, QByteArray> MapListModel::roleNames() const {
    QHash<int, QByteArray> names;
    for (int i = 0; i < m_roles.size(); ++i) names.insert(Qt::UserRole + 1 + i, m_roles[i].toUtf8());
    return names;
}

int MapListModel::indexOf(const QString &key) const {
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i].value(m_key).toString() == key) return i;
    return -1;
}

void MapListModel::upsert(const QVariantMap &values, int at) {
    const int i = indexOf(values.value(m_key).toString());
    if (i < 0) {
        const int pos = at < 0 || at > m_rows.size() ? int(m_rows.size()) : at;
        beginInsertRows({}, pos, pos);
        m_rows.insert(pos, values);
        endInsertRows();
        emit countChanged();
        return;
    }
    QList<int> changed;
    QVariantMap &row = m_rows[i];
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (row.value(it.key()) == it.value()) continue;
        row.insert(it.key(), it.value());
        const int r = int(m_roles.indexOf(it.key()));
        if (r >= 0) changed << Qt::UserRole + 1 + r;
    }
    if (!changed.isEmpty()) emit dataChanged(index(i), index(i), changed);
}

void MapListModel::remove(const QString &key) {
    const int i = indexOf(key);
    if (i < 0) return;
    beginRemoveRows({}, i, i);
    m_rows.removeAt(i);
    endRemoveRows();
    emit countChanged();
}

void MapListModel::moveToTop(const QString &key) {
    const int i = indexOf(key);
    if (i <= 0) return;
    beginMoveRows({}, i, i, {}, 0);
    m_rows.move(i, 0);
    endMoveRows();
}
