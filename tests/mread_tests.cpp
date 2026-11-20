#include <QDir>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextTable>
#include <QtTest>

#include "document.h"
#include "fileindex.h"
#include "fuzzymodel.h"
#include "markdownview.h"
#include "settings.h"
#include "theme.h"

class MreadTests : public QObject {
    Q_OBJECT

private slots:
    void init() { QVERIFY(m_dir.isValid()); }

    // ---- fuzzy matching ----------------------------------------------------

    void fuzzyMatchesInOrder() {
        QVERIFY(FuzzyModel::fuzzyMatch("rdm", "readme.md", nullptr));
        QVERIFY(!FuzzyModel::fuzzyMatch("mdr", "readme.md", nullptr));
        QVERIFY(FuzzyModel::fuzzyMatch("", "anything", nullptr));
    }

    void fuzzyUsesSmartCase() {
        QVERIFY(FuzzyModel::fuzzyMatch("readme", "README.md", nullptr));
        QVERIFY(!FuzzyModel::fuzzyMatch("ReadMe", "README.md", nullptr));
    }

    void fuzzyRequiresEveryTerm() {
        QVERIFY(FuzzyModel::fuzzyMatch("std cra", "~/Projects/STD/cra-summary.md", nullptr));
        QVERIFY(!FuzzyModel::fuzzyMatch("std xyz", "~/Projects/STD/cra-summary.md", nullptr));
    }

    void fuzzyRanksFileNamesAndRuns() {
        FuzzyModel model;
        const QStringList items{"~/notes/old/plan-archive.md", "~/a/p/l/a/n.md", "~/notes/plan.md"};
        model.setItems(items, items);
        model.setQuery("plan");
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.valueAt(0), QString("~/notes/plan.md"));
        QCOMPARE(model.valueAt(2), QString("~/a/p/l/a/n.md"));
    }

    void fuzzyKeepsOrderWithoutQuery() {
        FuzzyModel model;
        model.setItems({"b", "a", "c"}, {"b", "a", "c"});
        QCOMPARE(model.valueAt(0), QString("b"));
        model.setQuery("x");
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.valueAt(0), QString());
    }

    // ---- settings ----------------------------------------------------------

    void settingsClampAndPersist() {
        const QString file = m_dir.filePath("settings.conf");
        {
            Settings settings(file);
            QCOMPARE(settings.fontSize(), Settings::DefaultFontSize);
            settings.setFontSize(1000);
            QCOMPARE(settings.fontSize(), Settings::MaxFontSize);
            settings.setColumns(10);
            QCOMPARE(settings.columns(), Settings::MinColumns);
            settings.setBodyFont("Some Font");
        }
        Settings again(file);
        QCOMPARE(again.fontSize(), Settings::MaxFontSize);
        QCOMPARE(again.bodyFont(), QString("Some Font"));
        again.resetFontSize();
        QCOMPARE(again.fontSize(), Settings::DefaultFontSize);
    }

    void recentFilesAreUniqueAndNewestFirst() {
        Settings settings(m_dir.filePath("recent.conf"));
        settings.addRecent("/a.md");
        settings.addRecent("/b.md");
        settings.addRecent("/a.md");
        QCOMPARE(settings.recentFiles(), QStringList({"/a.md", "/b.md"}));
    }

    // ---- theme -------------------------------------------------------------

    void themeFallsBackWithoutOmarchy() {
        Theme theme(m_dir.filePath("missing"));
        QCOMPARE(theme.accent(), QColor("#59acf5"));
        QVERIFY(theme.dark());
    }

    void themeReadsEveryColorAndFollowsChanges() {
        QDir(m_dir.path()).mkpath("current/theme");
        const QString colors = m_dir.filePath("current/theme/colors.toml");
        write(colors, "mode = \"light\"\naccent = \"#112233\"\nbackground = '#ffffff'\nred = \"#ff0000\"\n");
        Theme theme(m_dir.filePath("current"));
        QCOMPARE(theme.accent(), QColor("#112233"));
        QCOMPARE(theme.background(), QColor("#ffffff"));
        QCOMPARE(theme.color("red"), QColor("#ff0000"));
        QVERIFY(!theme.dark());

        // Omarchy swaps themes by replacing files.
        QFile::remove(colors);
        write(colors, "accent = \"#445566\"\n");
        QTRY_COMPARE(theme.accent(), QColor("#445566"));
        QVERIFY(theme.dark());
    }

    // ---- document ----------------------------------------------------------

    void documentRendersAndStyles() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("doc.conf"));
        Document document(&theme, &settings);
        const QString path = m_dir.filePath("doc.md");
        write(path, "# Title\n\nText with `code`.\n\n## Two\n\n```sh\necho hi\nls\n```\n\n"
                    "| a | b |\n|---|---|\n| 1 | 2 |\n\n---\n\n### Three\n");
        QSignalSpy rendered(&document, &Document::rendered);
        QVERIFY(document.open(path));
        QCOMPARE(rendered.count(), 1);
        QCOMPARE(document.title(), QString("doc.md"));
        QCOMPARE(document.headings().size(), 3);
        QCOMPARE(settings.recentFiles().value(0), QFileInfo(path).canonicalFilePath());

        const QTextDocument *doc = document.textDocument();
        // Headings take explicit sizes, which the importer's adjustment would override.
        const QTextBlock title = doc->findBlock(document.headings()[0].toInt());
        QCOMPARE(title.text(), QString("Title"));
        QCOMPARE(title.begin().fragment().charFormat().font().pixelSize(), settings.fontSize() * 2);

        // Code block, table, and rule each sit in a frame of their own.
        const auto frames = doc->rootFrame()->childFrames();
        QCOMPARE(frames.size(), 3);
        QCOMPARE(doc->findBlock(frames[0]->firstPosition()).text(), QString("echo hi"));
        QVERIFY(qobject_cast<QTextTable *>(frames[1]));
        QCOMPARE(qobject_cast<QTextTable *>(frames[1])->cellAt(0, 0).firstCursorPosition().block().text(),
                 QString("a"));
    }

    void documentFindsWithSmartCaseAndWraps() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("find.conf"));
        Document document(&theme, &settings);
        const QString path = m_dir.filePath("find.md");
        write(path, "alpha Beta\n\nbeta gamma\n");
        QVERIFY(document.open(path));
        const auto first = document.find("beta", 0, false);
        QCOMPARE(first[0].toInt(), 6);
        const auto second = document.find("beta", first[1].toInt(), false);
        QCOMPARE(document.textDocument()->findBlock(second[0].toInt()).text(), QString("beta gamma"));
        // Past the last match it wraps to the first.
        QCOMPARE(document.find("beta", second[1].toInt(), false)[0].toInt(), 6);
        // Uppercase makes the search case sensitive.
        QCOMPARE(document.find("Beta", second[1].toInt(), false)[0].toInt(), 6);
        QCOMPARE(document.find("zzz", 0, false)[0].toInt(), -1);
    }

    void documentFollowsAnchorsAndMarkdownLinks() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("links.conf"));
        Document document(&theme, &settings);
        write(m_dir.filePath("one.md"), "# One\n\n## Getting Started!\n\n[two](two.md#end)\n");
        write(m_dir.filePath("two.md"), "# Two\n\ntext\n\n## End\n");
        QVERIFY(document.open(m_dir.filePath("one.md")));
        QCOMPARE(Document::slug("Getting Started!"), QString("getting-started"));
        QCOMPARE(document.followLink("#getting-started"), document.headings()[1].toInt());
        QCOMPARE(document.followLink("#nope"), -1);
        const int position = document.followLink("two.md#end");
        QCOMPARE(document.title(), QString("two.md"));
        QCOMPARE(position, document.headings()[1].toInt());
    }

    void documentReloadsWhenTheFileIsReplaced() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("reload.conf"));
        Document document(&theme, &settings);
        const QString path = m_dir.filePath("live.md");
        write(path, "# Before\n");
        QVERIFY(document.open(path));
        QSignalSpy rendered(&document, &Document::rendered);

        // Write beside and rename over, as editors do.
        write(path + ".tmp", "# After\n");
        QFile::remove(path);
        QFile::rename(path + ".tmp", path);
        QTRY_COMPARE(rendered.count(), 1);
        QCOMPARE(rendered.first().first().toBool(), true);
        QCOMPARE(document.textDocument()->begin().text(), QString("After"));
    }

    void documentReportsUnreadableFilesAndCloses() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("close.conf"));
        Document document(&theme, &settings);
        QVERIFY(!document.open(m_dir.filePath("absent.md")));
        QVERIFY(!document.error().isEmpty());
        QVERIFY(!document.isOpen());

        write(m_dir.filePath("c.md"), "text\n");
        QVERIFY(document.open(m_dir.filePath("c.md")));
        QVERIFY(document.error().isEmpty());
        document.close();
        QVERIFY(!document.isOpen());
        QVERIFY(document.textDocument()->isEmpty());
    }

    void documentRerendersOnSettingsChange() {
        Theme theme(m_dir.filePath("missing"));
        Settings settings(m_dir.filePath("rerender.conf"));
        Document document(&theme, &settings);
        write(m_dir.filePath("r.md"), "# Head\n");
        QVERIFY(document.open(m_dir.filePath("r.md")));
        settings.setFontSize(20);
        QCOMPARE(document.textDocument()->begin().begin().fragment().charFormat().font().pixelSize(), 40);
    }

    // ---- file index --------------------------------------------------------

    void fileIndexListsMarkdownWithRecentFirst() {
        if (QStandardPaths::findExecutable("fd").isEmpty())
            QSKIP("fd is not installed");
        QDir(m_dir.path()).mkpath("tree/sub");
        write(m_dir.filePath("tree/a.md"), "a");
        write(m_dir.filePath("tree/sub/b.markdown"), "b");
        write(m_dir.filePath("tree/c.txt"), "c");
        Settings settings(m_dir.filePath("index.conf"));
        settings.addRecent(m_dir.filePath("tree/sub/b.markdown"));
        settings.addRecent(m_dir.filePath("gone.md")); // Missing files are left out.

        FileIndex index(&settings, {m_dir.filePath("tree")});
        index.refresh();
        QTRY_COMPARE(index.model()->total(), 2);
        QCOMPARE(index.model()->valueAt(0), m_dir.filePath("tree/sub/b.markdown"));
        QCOMPARE(index.model()->valueAt(1), m_dir.filePath("tree/a.md"));
        QVERIFY(index.error().isEmpty());
    }

    void fileIndexSaysWhenFdIsMissing() {
        Settings settings(m_dir.filePath("nofd.conf"));
        FileIndex index(&settings, {m_dir.path()}, nullptr, "/nonexistent/fd");
        index.refresh();
        QTRY_VERIFY(!index.error().isEmpty());
    }

    // ---- search highlights in tables ----------------------------------------

    // Table cells are stored row by row, so a match at the end of a long
    // wrapped cell sits lower on screen than a match at the start of the next
    // cell. Highlighting must not assume match y grows in document order.
    void highlightsVisibleMatchAfterLowerOneInSameRow() {
        TableView t(this, 300);
        // View from just above the short cell's match to above the long
        // cell's match, which is below the bottom edge.
        t.view.setSize(QSizeF(1000, t.longY - t.shortY - 40));
        t.view.setContentY(t.shortY - 10);
        QVERIFY2(t.highlightChangesPixels(), "visible match in the next cell is not highlighted");
    }

    void highlightsVisibleMatchInRowTallerThanSlack() {
        TableView t(this, 2000);
        // The row is far taller than the 200 px look-behind: the short cell's
        // match is well above the view while the long cell's match is in it.
        QVERIFY2(t.longY - t.shortY > 400, "test row is not tall enough");
        t.view.setSize(QSizeF(1000, 200));
        t.view.setContentY(t.longY - 50);
        QVERIFY2(t.highlightChangesPixels(), "visible match at the end of a tall cell is not highlighted");
    }

private:
    // A document with one table row: a long cell ending in MARK and a short
    // cell starting with MARK, shown in a MarkdownView. y values are in
    // content coordinates, as MarkdownView::yOf returns them.
    struct TableView {
        TableView(MreadTests *tests, int fillerWords)
            : theme(tests->m_dir.filePath("missing")), settings(tests->m_dir.filePath("table.conf")),
              document(&theme, &settings) {
            QByteArray filler;
            for (int i = 0; i < fillerWords; ++i)
                filler += "word ";
            const QString path = tests->m_dir.filePath(QString("table-%1.md").arg(fillerWords));
            write(path, "# Table\n\n| Long | Short |\n|------|-------|\n| " + filler + "MARK | MARK here |\n");
            view.setSize(QSizeF(1000, 800));
            view.setTextWidth(760);
            view.setDocument(&document);
            document.open(path);
            const auto first = document.find("MARK", 0, false);
            const auto second = document.find("MARK", first[1].toInt(), false);
            longY = view.yOf(first[0].toInt());
            shortY = view.yOf(second[0].toInt());
        }

        bool highlightChangesPixels() {
            auto render = [this] {
                QImage image(view.size().toSize(), QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::black);
                QPainter painter(&image);
                view.paint(&painter);
                return image;
            };
            view.setHighlight({});
            const QImage plain = render();
            view.setHighlight("MARK");
            return render() != plain;
        }

        Theme theme;
        Settings settings;
        Document document;
        MarkdownView view;
        qreal longY = 0, shortY = 0;
    };

    static void write(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    QTemporaryDir m_dir;
};

QTEST_MAIN(MreadTests)
#include "mread_tests.moc"
