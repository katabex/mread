#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTextDocument>
#include <QTimer>
#include <QVariantList>

class Settings;
class Theme;

// The open Markdown file. It parses into a QTextDocument, which MarkdownView
// paints, and styles it with the Omarchy palette and the chosen fonts, and
// re-renders when the file, the theme, or the settings change.
class Document : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString title READ title NOTIFY pathChanged)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY pathChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList headings READ headings NOTIFY rendered)

public:
    Document(Theme *theme, Settings *settings, QObject *parent = nullptr);

    QString path() const { return m_path; }
    QString title() const;
    bool isOpen() const { return !m_path.isEmpty(); }
    QString error() const { return m_error; }
    QVariantList headings() const { return m_headings; }

    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void close();
    // Opens the file in Omarchy's default editor (omarchy-launch-editor).
    // Mread keeps showing it and reloads when the editor saves.
    Q_INVOKABLE bool edit();
    // Tests swap in a fake launcher.
    void setEditorCommand(const QString &command) { m_editorCommand = command; }

    // Next match of text from position, as [start, end], or [-1, -1].
    // Lowercase text ignores case, like vim's smartcase.
    Q_INVOKABLE QVariantList find(const QString &text, int from, bool backward) const;

    // Handles a clicked link: other Markdown files open here, "#anchor"
    // returns the heading position to scroll to, the rest go to the desktop.
    // Returns a position to scroll to, or -1.
    Q_INVOKABLE int followLink(const QString &link);

    QTextDocument *textDocument() { return &m_document; }
    const QTextDocument *textDocument() const { return &m_document; }

    static QString slug(const QString &heading);

signals:
    void pathChanged();
    void errorChanged();
    // Around every render, so QML can keep the reading position across
    // reloads, font changes, and theme switches.
    void aboutToRender();
    void rendered(bool reload);

private:
    void render(bool reload);
    void reloadFromDisk();
    void watch();
    void setError(const QString &error);

    Theme *m_theme;
    Settings *m_settings;
    QTextDocument m_document;
    QString m_path;
    QString m_editorCommand = "omarchy-launch-editor";
    QString m_markdown;
    QString m_error;
    QVariantList m_headings;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
};
