#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

// A list of QVariantMap rows whose keys are the QML roles. Rows are matched
// by the value of `keyRole`, so updates animate in place in a ListView.
class MapListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    MapListModel(const QStringList &roles, const QString &keyRole, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }
    int indexOf(const QString &key) const;
    QVariantMap row(int i) const { return m_rows.value(i); }
    const QList<QVariantMap> &rows() const { return m_rows; }

    // Inserts at `at` (-1: append) or merges `values` into the row with the same key.
    void upsert(const QVariantMap &values, int at = -1);
    void remove(const QString &key);
    void moveToTop(const QString &key);

signals:
    void countChanged();

private:
    QStringList m_roles;
    QString m_key;
    QList<QVariantMap> m_rows;
};
