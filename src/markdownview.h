#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTextDocument>

#include "document.h"

// Paints the visible slice of the rendered document, centered in a column.
// Painting only what is on screen keeps scrolling cheap for long files, and
// owning the paint lets search highlight every match, as vim's hlsearch does.
class MarkdownView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Document *source READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
    Q_PROPERTY(qreal textWidth READ textWidth WRITE setTextWidth NOTIFY textWidthChanged)
    Q_PROPERTY(qreal margin MEMBER m_margin NOTIFY contentHeightChanged)
    Q_PROPERTY(qreal wheelStep MEMBER m_wheelStep)
    Q_PROPERTY(QColor textColor MEMBER m_textColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor selectionColor MEMBER m_selectionColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor matchColor MEMBER m_matchColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor currentMatchColor MEMBER m_currentMatchColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor currentMatchTextColor MEMBER m_currentMatchTextColor NOTIFY colorsChanged)
    Q_PROPERTY(QString highlight READ highlight WRITE setHighlight NOTIFY highlightChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

public:
    explicit MarkdownView(QQuickItem *parent = nullptr);

    Document *document() const { return m_document; }
    void setDocument(Document *document);
    qreal contentY() const { return m_contentY; }
    void setContentY(qreal y);
    qreal contentHeight() const;
    qreal textWidth() const { return m_textWidth; }
    void setTextWidth(qreal width);
    QString highlight() const { return m_highlight; }
    void setHighlight(const QString &text);
    bool hasSelection() const { return m_selectionStart != m_selectionEnd; }

    // Positions are document positions; y values are in content coordinates.
    Q_INVOKABLE qreal yOf(int position) const;
    Q_INVOKABLE int positionAtY(qreal y) const;
    // A search match is painted as the current match; a mouse selection
    // as a plain selection.
    Q_INVOKABLE void select(int start, int end, bool match = false);
    Q_INVOKABLE void deselect() { select(0, 0); }
    Q_INVOKABLE int selectionStart() const { return qMin(m_selectionStart, m_selectionEnd); }
    Q_INVOKABLE int selectionEnd() const { return qMax(m_selectionStart, m_selectionEnd); }
    Q_INVOKABLE QString selectedText() const;
    Q_INVOKABLE void copy() const;

    void paint(QPainter *painter) override;

signals:
    void documentChanged();
    void contentYChanged();
    void contentHeightChanged();
    void textWidthChanged();
    void colorsChanged();
    void highlightChanged();
    void selectionChanged();
    void linkActivated(const QString &link);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QTextDocument *textDocument() const;
    QPair<qreal, qreal> matchRange(int position) const;
    QPointF toDocument(QPointF point) const;
    int hitTest(QPointF point) const;
    qreal maxContentY() const;
    void refreshMatches();

    QPointer<Document> m_document;
    qreal m_contentY = 0;
    qreal m_textWidth = 600;
    qreal m_margin = 40;
    qreal m_wheelStep = 60;
    QColor m_textColor = Qt::white;
    QColor m_selectionColor = QColor("#404f6c");
    QColor m_matchColor = QColor("#29364f");
    QColor m_currentMatchColor = QColor("#59acf5");
    QColor m_currentMatchTextColor = QColor("#131c2f");
    bool m_selectionIsMatch = false;
    QString m_highlight;
    QList<QPair<int, int>> m_matches;
    int m_selectionStart = 0, m_selectionEnd = 0;
    QString m_pressedLink;
    bool m_dragged = false;
};
