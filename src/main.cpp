// MreaD - a minimal, keyboard-driven Markdown reader for Omarchy.

#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "document.h"
#include "fileindex.h"
#include "fuzzymodel.h"
#include "markdownview.h"
#include "settings.h"
#include "theme.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("mread");
    app.setApplicationDisplayName("MreaD");
    app.setApplicationVersion("0.1.0");

    // Matches the window to mread.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("mread");
    app.setWindowIcon(QIcon::fromTheme("mread"));

    QCommandLineParser parser;
    parser.setApplicationDescription("A minimal Markdown reader with vim keys.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Markdown file to open.", "[file]");
    parser.process(app);

    QQuickStyle::setStyle("Material");

    Theme theme;
    Settings settings;
    Document document(&theme, &settings);

    // The finder searches the home directory, and the directory mread was
    // started in when that lies outside it.
    QStringList roots{QDir::homePath()};
    const QString cwd = QDir::currentPath();
    if (cwd != QDir::homePath() && !cwd.startsWith(QDir::homePath() + '/'))
        roots << cwd;
    FileIndex files(&settings, roots);

    FuzzyModel fonts;
    const QStringList families = QFontDatabase::families();
    fonts.setItems(families, families);

    QObject::connect(&theme, &Theme::changed, &app, [&] {
        files.model()->setProperty("highlightColor", theme.accent());
        fonts.setProperty("highlightColor", theme.accent());
    });
    files.model()->setProperty("highlightColor", theme.accent());
    fonts.setProperty("highlightColor", theme.accent());

    qmlRegisterType<MarkdownView>("Mread", 1, 0, "MarkdownView");
    qmlRegisterUncreatableType<Document>("Mread", 1, 0, "Document", "Use the document context property");

    QQmlApplicationEngine engine;
    auto *context = engine.rootContext();
    context->setContextProperty("theme", &theme);
    context->setContextProperty("settings", &settings);
    context->setContextProperty("document", &document);
    context->setContextProperty("fileIndex", &files);
    context->setContextProperty("fontModel", &fonts);
    context->setContextProperty("initialFile",
                                parser.positionalArguments().value(0));
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
