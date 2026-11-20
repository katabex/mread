#include "fuzzymodel.h"

#include <algorithm>

namespace {

constexpr int MaxRows = 300;

bool isSeparator(QChar c) { return c == '/' || c == '-' || c == '_' || c == '.' || c == ' '; }

bool equalChar(QChar a, QChar b, bool caseSensitive) {
    return caseSensitive ? a == b : a.toLower() == b.toLower();
}

bool matchTerm(const QString &term, const QString &text, int *score, QVector<int> *positions) {
    // Smart case, as in vim and fzf: lowercase queries ignore case.
    const bool caseSensitive = std::any_of(term.begin(), term.end(), [](QChar c) { return c.isUpper(); });

    // Forward pass finds where the earliest complete match ends ...
    int t = 0, end = -1;
    for (int i = 0; i < text.size() && t < term.size(); ++i)
        if (equalChar(text[i], term[t], caseSensitive) && ++t == term.size())
            end = i;
    if (end < 0)
        return false;

    // ... and a backward pass from there finds the tightest window, which is
    // what a person usually means.
    QVector<int> found(term.size());
    t = term.size() - 1;
    for (int i = end; i >= 0 && t >= 0; --i)
        if (equalChar(text[i], term[t], caseSensitive))
            found[t--] = i;

    const int lastSlash = text.lastIndexOf('/');
    int total = 0;
    for (int k = 0; k < found.size(); ++k) {
        const int i = found[k];
        int s = 16;
        if (i == 0 || isSeparator(text[i - 1]))
            s += (i > 0 && text[i - 1] == '/') ? 10 : 8;
        else if (text[i].isUpper() && text[i - 1].isLower())
            s += 7;
        if (k > 0) {
            const int gap = i - found[k - 1] - 1;
            s += gap == 0 ? 6 : -(3 + std::min(gap - 1, 12));
        }
        if (i > lastSlash)
            s += 4;
        total += s;
    }
    *score += total;
    *positions += found;
    return true;
}

} // namespace

FuzzyModel::FuzzyModel(QObject *parent) : QAbstractListModel(parent) {
    connect(this, &FuzzyModel::highlightChanged, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1), {HighlightedRole});
    });
}

bool FuzzyModel::fuzzyMatch(const QString &query, const QString &text, Match *result) {
    Match m;
    const auto terms = query.split(' ', Qt::SkipEmptyParts);
    for (const auto &term : terms)
        if (!matchTerm(term, text, &m.score, &m.positions))
            return false;
    std::sort(m.positions.begin(), m.positions.end());
    m.positions.erase(std::unique(m.positions.begin(), m.positions.end()), m.positions.end());
    if (result)
        *result = m;
    return true;
}

int FuzzyModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }

QVariant FuzzyModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    const QString &label = m_labels[row.item];
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return label;
    case ValueRole:
        return m_values[row.item];
    case HighlightedRole: {
        // Rich text with the matched characters in the accent color.
        QString html;
        int next = 0;
        const QString open = QStringLiteral("<b><font color=\"%1\">").arg(m_highlight.name());
        for (int i = 0; i < label.size(); ++i) {
            const bool hit = next < row.match.positions.size() && row.match.positions[next] == i;
            if (hit)
                ++next;
            const QString ch = QString(label[i]).toHtmlEscaped();
            html += hit ? open + ch + "</font></b>" : ch;
        }
        return html;
    }
    }
    return {};
}

QHash<int, QByteArray> FuzzyModel::roleNames() const {
    return {{LabelRole, "label"}, {ValueRole, "value"}, {HighlightedRole, "highlighted"}};
}

void FuzzyModel::setQuery(const QString &query) {
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
    refilter();
}

void FuzzyModel::setItems(const QStringList &labels, const QStringList &values) {
    m_labels = labels;
    m_values = values.size() == labels.size() ? values : labels;
    refilter();
}

QString FuzzyModel::valueAt(int row) const {
    return row >= 0 && row < m_rows.size() ? m_values[m_rows[row].item] : QString();
}

void FuzzyModel::refilter() {
    QVector<Row> rows;
    if (m_query.trimmed().isEmpty()) {
        // Without a query the given order stands (recent files come first).
        for (int i = 0; i < m_labels.size() && rows.size() < MaxRows; ++i)
            rows.append(Row{i, Match{}});
    } else {
        for (int i = 0; i < m_labels.size(); ++i) {
            Match m;
            if (fuzzyMatch(m_query, m_labels[i], &m))
                rows.append(Row{i, m});
        }
        std::stable_sort(rows.begin(), rows.end(), [this](const Row &a, const Row &b) {
            if (a.match.score != b.match.score)
                return a.match.score > b.match.score;
            return m_labels[a.item].size() < m_labels[b.item].size();
        });
        if (rows.size() > MaxRows)
            rows.resize(MaxRows);
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
    emit countChanged();
}
