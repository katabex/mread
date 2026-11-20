#include "document.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFrame>
#include <QTextTable>
#include <QUrl>

#include "settings.h"
#include "theme.h"

namespace {

// Heading sizes relative to the body text, h1 to h6.
constexpr double HeadingScale[] = {2.0, 1.6, 1.35, 1.15, 1.0, 0.9};

// Fenced blocks carry a fence and a language; indented ones only an empty
// language.
bool isCodeBlock(const QTextBlockFormat &format) {
    return format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
}

// Marks the single block inside a horizontal rule's frame.
constexpr int RuleProperty = QTextFormat::UserProperty + 1;

// A run of blocks that goes into a frame of its own: a code block, or a
// horizontal rule (Qt Quick draws its built-in rule in the text color, too
// loud for a divider, so rules become thin filled frames instead).
struct FramedRun {
    int start, end;
    QTextBlockFormat format;
    bool rule;
};

bool sameCodeBlock(const QTextBlockFormat &a, const QTextBlockFormat &b) {
    return a.hasProperty(QTextFormat::BlockCodeFence) == b.hasProperty(QTextFormat::BlockCodeFence)
        && a.stringProperty(QTextFormat::BlockCodeLanguage) == b.stringProperty(QTextFormat::BlockCodeLanguage);
}

void styleTables(QTextFrame *frame, const Theme &theme, int px) {
    for (QTextFrame *child : frame->childFrames()) {
        if (auto *table = qobject_cast<QTextTable *>(child)) {
            QTextTableFormat format = table->format();
            format.setBorder(1);
            format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
            format.setBorderBrush(theme.lighterBackground());
            format.setBorderCollapse(true);
            format.setCellSpacing(0);
            format.setCellPadding(px * 0.4);
            format.setMargin(0);
            format.setTopMargin(px * 0.5);
            format.setBottomMargin(px * 0.5);
            table->setFormat(format);
            // Drop empty leading blocks in cells, which the importer leaves
            // in the first one.
            for (int row = 0; row < table->rows(); ++row)
                for (int column = 0; column < table->columns(); ++column) {
                    const QTextTableCell cell = table->cellAt(row, column);
                    QTextCursor first = cell.firstCursorPosition();
                    while (first.block().length() == 1 && first.position() < cell.lastPosition())
                        first.deleteChar();
                }
            // The header row is bold in the accent color.
            for (int column = 0; column < table->columns(); ++column) {
                const QTextTableCell cell = table->cellAt(0, column);
                QTextCursor text = cell.firstCursorPosition();
                text.setPosition(cell.lastPosition(), QTextCursor::KeepAnchor);
                QTextCharFormat header;
                header.setFontWeight(QFont::Bold);
                header.setForeground(theme.accent());
                text.mergeCharFormat(header);
            }
        }
        styleTables(child, theme, px);
    }
}

} // namespace

Document::Document(Theme *theme, Settings *settings, QObject *parent)
    : QObject(parent), m_theme(theme), m_settings(settings) {
    m_document.setUndoRedoEnabled(false);
    // Editors save by replacing the file, so changes arrive in bursts and the
    // file may briefly be missing; wait for the dust to settle.
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(120);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_reloadTimer, &QTimer::timeout, this, &Document::reloadFromDisk);
    connect(m_theme, &Theme::changed, this, [this] { render(true); });
    connect(m_settings, &Settings::changed, this, [this] { render(true); });
}

QString Document::title() const { return m_path.isEmpty() ? QString() : QFileInfo(m_path).fileName(); }

bool Document::open(const QString &path) {
    const QFileInfo info(path);
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        setError(tr("Cannot open %1: %2").arg(path, file.errorString()));
        return false;
    }
    m_markdown = QString::fromUtf8(file.readAll());
    m_path = info.canonicalFilePath();
    setError({});
    m_settings->addRecent(m_path);
    watch();
    emit pathChanged();
    render(false);
    return true;
}

void Document::close() {
    if (m_path.isEmpty())
        return;
    m_path.clear();
    m_markdown.clear();
    watch();
    setError({});
    emit pathChanged();
    render(false);
}

void Document::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    if (m_path.isEmpty())
        return;
    // The directory catches atomic saves, which replace the watched file.
    m_watcher.addPath(QFileInfo(m_path).absolutePath());
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
}

void Document::reloadFromDisk() {
    if (m_path.isEmpty())
        return;
    if (!m_watcher.files().contains(m_path) && QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return; // Mid-save or deleted: keep showing what was there.
    const QString markdown = QString::fromUtf8(file.readAll());
    if (markdown == m_markdown)
        return;
    m_markdown = markdown;
    render(true);
}

void Document::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

QString Document::slug(const QString &heading) {
    // GitHub's anchor rules: lowercase, punctuation dropped, spaces to dashes.
    QString result;
    for (QChar c : heading.trimmed().toLower()) {
        if (c.isLetterOrNumber() || c == '-' || c == '_')
            result += c;
        else if (c.isSpace())
            result += '-';
    }
    return result;
}

void Document::render(bool reload) {
    QTextDocument *doc = textDocument();
    emit aboutToRender();
    m_headings.clear();
    if (m_path.isEmpty()) {
        doc->clear();
        emit rendered(reload);
        return;
    }

    const int px = m_settings->fontSize();
    QFont body(m_settings->bodyFont());
    body.setPixelSize(px);
    doc->setDefaultFont(body);
    doc->setBaseUrl(QUrl::fromLocalFile(QFileInfo(m_path).absolutePath() + "/"));
    doc->setMarkdown(m_markdown, QTextDocument::MarkdownDialectGitHub);

    const Theme &theme = *m_theme;
    QTextCursor cursor(doc);
    cursor.beginEditBlock();

    // Tables first: the importer leaves an empty block at the start of the
    // first cell, which pushes the header row out of line.
    styleTables(doc->rootFrame(), theme, px);

    // Each code block goes into a padded frame on a darker band. Frames shift
    // later positions, so collect the blocks first and wrap from the end.
    QList<FramedRun> runs;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat format = block.blockFormat();
        const int end = block.position() + block.length() - 1;
        if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            QTextBlockFormat line;
            line.setProperty(RuleProperty, true);
            line.setLineHeight(1, QTextBlockFormat::FixedHeight);
            runs.append({block.position(), end, line, true});
            continue;
        }
        if (!isCodeBlock(format))
            continue;
        if (!runs.isEmpty() && !runs.last().rule && runs.last().end + 1 == block.position()
            && sameCodeBlock(runs.last().format, format))
            runs.last().end = end;
        else
            runs.append({block.position(), end, format, false});
    }
    for (auto run = runs.crbegin(); run != runs.crend(); ++run) {
        cursor.setPosition(run->start);
        cursor.setPosition(run->end, QTextCursor::KeepAnchor);
        QTextFrameFormat frame;
        frame.setBackground(run->rule ? theme.lighterBackground() : theme.darkBackground());
        frame.setPadding(run->rule ? 0 : px * 0.75);
        frame.setMargin(0);
        frame.setBorder(0);
        if (run->rule)
            frame.setHeight(QTextLength(QTextLength::FixedLength, 1));
        QTextFrame *inserted = cursor.insertFrame(frame);
        // The frame's first block starts with a plain format; restore the
        // code block format on every line so the styling below finds them.
        for (QTextBlock block = doc->findBlock(inserted->firstPosition());
             block.isValid() && block.position() <= inserted->lastPosition(); block = block.next()) {
            QTextCursor line(block);
            line.setBlockFormat(run->format);
        }
    }

    // Which top-level frame each block sits in, found by walking the sorted
    // frames alongside the blocks; asking a cursor per block is quadratic.
    const QList<QTextFrame *> frames = doc->rootFrame()->childFrames();
    qsizetype frameIndex = 0;

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat blockFormat = block.blockFormat();
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        while (frameIndex < frames.size() && frames[frameIndex]->lastPosition() < block.position())
            ++frameIndex;
        const QTextFrame *frame = frameIndex < frames.size() && frames[frameIndex]->firstPosition() <= block.position()
                                    ? frames[frameIndex] : nullptr;
        const bool inTable = qobject_cast<const QTextTable *>(frame) != nullptr;
        if (blockFormat.boolProperty(RuleProperty)) {
            QTextCharFormat tiny;
            tiny.setProperty(QTextFormat::FontPixelSize, 1);
            cursor.setBlockCharFormat(tiny);
            continue;
        }

        // Qt needs an empty block between frames (code blocks, tables); keep
        // them as small plain spacers.
        if (block.length() == 1 && !block.textList() && !frame) {
            // Frames in a row leave a spacer on each side of the gap.
            QTextBlockFormat spacer;
            spacer.setLineHeight(px * 0.45, QTextBlockFormat::FixedHeight);
            cursor.setBlockFormat(spacer);
            QTextCharFormat tiny;
            tiny.setProperty(QTextFormat::FontPixelSize, 1);
            cursor.setBlockCharFormat(tiny);
            continue;
        }

        if (isCodeBlock(blockFormat)) {
            QTextCharFormat code;
            code.setFontFamilies({m_settings->codeFont()});
            code.setFontFixedPitch(true);
            code.setProperty(QTextFormat::FontPixelSize, qRound(px * 0.9));
            code.clearBackground();
            cursor.mergeCharFormat(code);
            // The block's own char format sizes empty lines in the code.
            QTextCharFormat blockChar = block.charFormat();
            blockChar.merge(code);
            cursor.setBlockCharFormat(blockChar);
            blockFormat.setLineHeight(115, QTextBlockFormat::ProportionalHeight);
            blockFormat.setTopMargin(0);
            blockFormat.setBottomMargin(0);
            cursor.setBlockFormat(blockFormat);
            continue;
        }

        const int level = blockFormat.headingLevel();
        if (level > 0) {
            // Replace rather than merge: the importer's size adjustment would
            // otherwise win over an explicit size.
            const int size = qRound(px * HeadingScale[qMin(level, 6) - 1]);
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                QTextCharFormat heading = fragment.charFormat();
                heading.clearProperty(QTextFormat::FontSizeAdjustment);
                heading.setProperty(QTextFormat::FontPixelSize, size);
                heading.setFontWeight(QFont::Bold);
                if (!heading.isAnchor())
                    heading.setForeground(theme.accent());
                QTextCursor span(doc);
                span.setPosition(fragment.position());
                span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                span.setCharFormat(heading);
            }
            blockFormat.setTopMargin(px * (level <= 2 ? 1.6 : 1.2));
            blockFormat.setBottomMargin(px * 0.5);
            cursor.setBlockFormat(blockFormat);
            continue;
        }

        blockFormat.setLineHeight(inTable ? 100 : 140, QTextBlockFormat::ProportionalHeight);
        if (inTable) {
            blockFormat.setTopMargin(0);
            blockFormat.setBottomMargin(0);
        } else if (!block.textList()) {
            blockFormat.setBottomMargin(px * 0.8);
        }
        if (blockFormat.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            QTextCharFormat quote;
            quote.setForeground(theme.muted());
            quote.setFontItalic(true);
            cursor.mergeCharFormat(quote);
        }
        cursor.setBlockFormat(blockFormat);

        // Inline spans: code, links, images.
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            const QTextCharFormat format = fragment.charFormat();
            QTextCursor span(doc);
            span.setPosition(fragment.position());
            span.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            if (format.isImageFormat()) {
                // Wide images shrink to the column instead of overflowing it.
                QTextImageFormat image = format.toImageFormat();
                image.setMaximumWidth(QTextLength(QTextLength::PercentageLength, 100));
                span.setCharFormat(image);
            } else if (format.isAnchor()) {
                QTextCharFormat link;
                link.setForeground(theme.accent());
                link.setFontUnderline(true);
                span.mergeCharFormat(link);
            } else if (format.fontFixedPitch()) {
                QTextCharFormat code;
                code.setFontFamilies({m_settings->codeFont()});
                code.setProperty(QTextFormat::FontPixelSize, qRound(px * 0.9));
                code.setBackground(theme.lighterBackground());
                span.mergeCharFormat(code);
            }
        }
    }
    cursor.endEditBlock();

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
        if (block.blockFormat().headingLevel() > 0)
            m_headings.append(block.position());

    emit rendered(reload);
}

QVariantList Document::find(const QString &text, int from, bool backward) const {
    const QTextDocument *doc = textDocument();
    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (std::any_of(text.begin(), text.end(), [](QChar c) { return c.isUpper(); }))
        flags |= QTextDocument::FindCaseSensitively;
    QTextCursor match = doc->find(text, from, flags);
    if (match.isNull()) // Wrap around, as vim does.
        match = doc->find(text, backward ? doc->characterCount() : 0, flags);
    if (match.isNull())
        return {-1, -1};
    return {match.selectionStart(), match.selectionEnd()};
}

int Document::followLink(const QString &link) {
    const QTextDocument *doc = textDocument();
    auto headingPosition = [doc](const QString &anchor) {
        const QString wanted = anchor.toLower();
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
            if (block.blockFormat().headingLevel() > 0 && slug(block.text()) == wanted)
                return block.position();
        return -1;
    };

    if (link.startsWith('#'))
        return headingPosition(link.mid(1));

    const QUrl url = doc->baseUrl().resolved(QUrl(link));
    if (url.isLocalFile()) {
        const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        if ((suffix == "md" || suffix == "markdown") && open(url.toLocalFile()))
            return url.hasFragment() ? headingPosition(url.fragment()) : -1;
    }
    QDesktopServices::openUrl(url);
    return -1;
}
