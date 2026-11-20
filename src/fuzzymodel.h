#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QStringList>
#include <QVector>

// Fuzzy matching over a list of strings, fzf style: every character of the
// query must appear in order; consecutive runs, word starts, and matches in
// the last path component score higher. Space-separated terms must all match.
class FuzzyModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY countChanged)
    Q_PROPERTY(QColor highlightColor MEMBER m_highlight NOTIFY highlightChanged)

public:
    enum Roles { LabelRole = Qt::UserRole + 1, ValueRole, HighlightedRole };

    struct Match {
        int score = 0;
        QVector<int> positions;
    };

    explicit FuzzyModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const { return m_query; }
    void setQuery(const QString &query);
    int total() const { return m_labels.size(); }

    // Labels are what is shown and matched; values are what is returned.
    void setItems(const QStringList &labels, const QStringList &values);
    Q_INVOKABLE QString valueAt(int row) const;

    // Returns false when the query does not match.
    static bool fuzzyMatch(const QString &query, const QString &text, Match *result);

signals:
    void queryChanged();
    void countChanged();
    void highlightChanged();

private:
    void refilter();

    struct Row {
        int item;
        Match match;
    };

    QString m_query;
    QStringList m_labels, m_values;
    QVector<Row> m_rows;
    QColor m_highlight = QColor("#59acf5");
};
