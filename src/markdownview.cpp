#include "markdownview.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

#include <algorithm>

#include "document.h"

namespace {
constexpr int MaxMatches = 5000;
}

MarkdownView::MarkdownView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setAntialiasing(true);
    setCursor(Qt::IBeamCursor);
    for (auto signal : {&MarkdownView::colorsChanged, &MarkdownView::textWidthChanged, &MarkdownView::selectionChanged})
        connect(this, signal, this, [this] { update(); });
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { setContentY(m_contentY); update(); });
}

QTextDocument *MarkdownView::textDocument() const {
    return m_document ? m_document->textDocument() : nullptr;
}

void MarkdownView::setDocument(Document *document) {
    if (document == m_document)
        return;
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);
    m_document = document;
    if (m_document) {
        connect(m_document, &Document::rendered, this, [this] {
            textDocument()->setTextWidth(m_textWidth);
            m_selectionStart = m_selectionEnd = 0;
            refreshMatches();
            emit contentHeightChanged();
            emit selectionChanged();
            setContentY(m_contentY);
            update();
        });
        textDocument()->setTextWidth(m_textWidth);
    }
    emit documentChanged();
    emit contentHeightChanged();
    update();
}

void MarkdownView::setContentY(qreal y) {
    y = qBound<qreal>(0, y, maxContentY());
    if (qFuzzyCompare(y + 1, m_contentY + 1))
        return;
    m_contentY = y;
    emit contentYChanged();
    update();
}

qreal MarkdownView::contentHeight() const {
    const QTextDocument *doc = textDocument();
    return (doc ? doc->size().height() : 0) + 2 * m_margin;
}

qreal MarkdownView::maxContentY() const { return qMax<qreal>(0, contentHeight() - height()); }

void MarkdownView::setTextWidth(qreal width) {
    width = qMax<qreal>(100, width);
    if (qFuzzyCompare(width, m_textWidth))
        return;
    m_textWidth = width;
    if (QTextDocument *doc = textDocument())
        doc->setTextWidth(width);
    emit textWidthChanged();
    emit contentHeightChanged();
    setContentY(m_contentY);
}

void MarkdownView::setHighlight(const QString &text) {
    if (text == m_highlight)
        return;
    m_highlight = text;
    refreshMatches();
    emit highlightChanged();
    update();
}

void MarkdownView::refreshMatches() {
    m_matches.clear();
    const QTextDocument *doc = textDocument();
    if (!doc || m_highlight.isEmpty())
        return;
    QTextDocument::FindFlags flags;
    if (std::any_of(m_highlight.begin(), m_highlight.end(), [](QChar c) { return c.isUpper(); }))
        flags |= QTextDocument::FindCaseSensitively;
    for (QTextCursor match = doc->find(m_highlight, 0, flags);
         !match.isNull() && m_matches.size() < MaxMatches; match = doc->find(m_highlight, match, flags))
        m_matches.append({match.selectionStart(), match.selectionEnd()});
}

qreal MarkdownView::yOf(int position) const {
    const QTextDocument *doc = textDocument();
    if (!doc)
        return 0;
    const QTextBlock block = doc->findBlock(position);
    if (!block.isValid())
        return 0;
    const QRectF rect = doc->documentLayout()->blockBoundingRect(block);
    qreal y = rect.y();
    if (const QTextLayout *layout = block.layout()) {
        const QTextLine line = layout->lineForTextPosition(position - block.position());
        if (line.isValid())
            y += layout->position().y() - rect.y() + line.y();
    }
    return y + m_margin;
}

int MarkdownView::positionAtY(qreal y) const {
    const QTextDocument *doc = textDocument();
    if (!doc)
        return 0;
    const int position = doc->documentLayout()->hitTest(QPointF(0, y - m_margin), Qt::FuzzyHit);
    return qMax(0, position);
}

void MarkdownView::select(int start, int end, bool match) {
    if (start == m_selectionStart && end == m_selectionEnd && match == m_selectionIsMatch)
        return;
    m_selectionIsMatch = match;
    m_selectionStart = start;
    m_selectionEnd = end;
    emit selectionChanged();
}

QString MarkdownView::selectedText() const {
    const QTextDocument *doc = textDocument();
    if (!doc || !hasSelection())
        return {};
    QTextCursor cursor(const_cast<QTextDocument *>(doc));
    cursor.setPosition(selectionStart());
    cursor.setPosition(selectionEnd(), QTextCursor::KeepAnchor);
    QString text = cursor.selectedText();
    text.replace(QChar::ParagraphSeparator, '\n').replace(QChar::LineSeparator, '\n');
    text.remove(QChar::Nbsp).remove(QChar(0xfdd0)).remove(QChar(0xfdd1)); // frame markers
    return text;
}

void MarkdownView::copy() const {
    const QString text = selectedText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

QPointF MarkdownView::toDocument(QPointF point) const {
    return {point.x() - qRound((width() - m_textWidth) / 2), point.y() + m_contentY - m_margin};
}

int MarkdownView::hitTest(QPointF point) const {
    const QTextDocument *doc = textDocument();
    return doc ? qMax(0, doc->documentLayout()->hitTest(toDocument(point), Qt::FuzzyHit)) : 0;
}

void MarkdownView::paint(QPainter *painter) {
    QTextDocument *doc = textDocument();
    if (!doc || !m_document->isOpen())
        return;
    const qreal x = qRound((width() - m_textWidth) / 2);
    painter->translate(x, m_margin - m_contentY);

    QAbstractTextDocumentLayout::PaintContext context;
    context.cursorPosition = -1;
    context.clip = QRectF(-x, m_contentY - m_margin, width(), height());
    context.palette.setColor(QPalette::Text, m_textColor);
    context.palette.setColor(QPalette::Dark, m_matchColor);

    // Matches first, then the selection on top, so the current match shows.
    const qreal top = m_contentY - m_margin, bottom = top + height();
    QTextCharFormat matchFormat;
    matchFormat.setBackground(m_matchColor);
    // Matches are in document order, so a binary search on their y finds the
    // first one near the viewport in O(log n) cheap yOf() calls.
    const auto first = std::lower_bound(m_matches.cbegin(), m_matches.cend(), top - 200,
        [this](const QPair<int, int> &match, qreal limit) { return yOf(match.first) - m_margin < limit; });
    for (auto it = first; it != m_matches.cend(); ++it) {
        const auto [start, end] = *it;
        const qreal y = yOf(start) - m_margin;
        if (y < top - 200)
            continue;
        if (y > bottom)
            break;
        QTextCursor cursor(doc);
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        context.selections.append({cursor, matchFormat});
    }
    if (hasSelection()) {
        QTextCursor cursor(doc);
        cursor.setPosition(selectionStart());
        cursor.setPosition(selectionEnd(), QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setBackground(m_selectionIsMatch ? m_currentMatchColor : m_selectionColor);
        if (m_selectionIsMatch)
            format.setForeground(m_currentMatchTextColor);
        context.selections.append({cursor, format});
    }

    doc->documentLayout()->draw(painter, context);
}

void MarkdownView::mousePressEvent(QMouseEvent *event) {
    const QTextDocument *doc = textDocument();
    if (!doc)
        return;
    m_dragged = false;
    m_pressedLink = doc->documentLayout()->anchorAt(toDocument(event->position()));
    const int position = hitTest(event->position());
    select(position, position);
    event->accept();
}

void MarkdownView::mouseMoveEvent(QMouseEvent *event) {
    m_dragged = true;
    // Dragging past the edges scrolls, so long selections are possible.
    if (event->position().y() < 0)
        setContentY(m_contentY + event->position().y());
    else if (event->position().y() > height())
        setContentY(m_contentY + event->position().y() - height());
    select(m_selectionStart, hitTest(event->position()));
}

void MarkdownView::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_dragged && !m_pressedLink.isEmpty()) {
        const QString link = m_pressedLink;
        m_pressedLink.clear();
        emit linkActivated(link);
    }
    event->accept();
}

void MarkdownView::mouseDoubleClickEvent(QMouseEvent *event) {
    QTextDocument *doc = textDocument();
    if (!doc)
        return;
    QTextCursor cursor(doc);
    cursor.setPosition(hitTest(event->position()));
    cursor.select(QTextCursor::WordUnderCursor);
    select(cursor.selectionStart(), cursor.selectionEnd());
}

void MarkdownView::hoverMoveEvent(QHoverEvent *event) {
    const QTextDocument *doc = textDocument();
    const bool onLink = doc && !doc->documentLayout()->anchorAt(toDocument(event->position())).isEmpty();
    setCursor(onLink ? Qt::PointingHandCursor : Qt::IBeamCursor);
}

void MarkdownView::wheelEvent(QWheelEvent *event) {
    // Touchpads report pixels; mouse wheels report notches of 120.
    const qreal delta = !event->pixelDelta().isNull() ? event->pixelDelta().y()
                                                      : event->angleDelta().y() / 120.0 * m_wheelStep;
    setContentY(m_contentY - delta);
    event->accept();
}
