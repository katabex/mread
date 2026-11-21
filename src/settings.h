#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

// Reader preferences and the reading session (the last open file and the
// position at the top of the window), saved to ~/.config/mread/mread.conf.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString bodyFont READ bodyFont WRITE setBodyFont NOTIFY changed)
    Q_PROPERTY(QString codeFont READ codeFont WRITE setCodeFont NOTIFY changed)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY changed)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY changed)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentChanged)
    Q_PROPERTY(QString lastFile READ lastFile CONSTANT)
    Q_PROPERTY(int lastPosition READ lastPosition CONSTANT)
    Q_PROPERTY(qreal lastOffset READ lastOffset CONSTANT)

public:
    static constexpr int DefaultFontSize = 16;
    static constexpr int DefaultColumns = 80;
    static constexpr int MinFontSize = 8, MaxFontSize = 48;
    static constexpr int MinColumns = 40, MaxColumns = 200;

    explicit Settings(const QString &file = {}, QObject *parent = nullptr);

    QString bodyFont() const;
    QString codeFont() const;
    int fontSize() const;
    int columns() const;
    QStringList recentFiles() const;
    QString lastFile() const;
    int lastPosition() const;
    qreal lastOffset() const;

    void setBodyFont(const QString &family);
    void setCodeFont(const QString &family);
    void setFontSize(int size);
    void setColumns(int columns);

    Q_INVOKABLE void resetFontSize() { setFontSize(DefaultFontSize); }
    Q_INVOKABLE void resetColumns() { setColumns(DefaultColumns); }
    Q_INVOKABLE void addRecent(const QString &path);
    // The position is the document character at the top of the window, which
    // stays right when the font size or column width changes; the offset is
    // how far, in pixels, its line was scrolled past the top. An empty path
    // forgets the file.
    Q_INVOKABLE void saveSession(const QString &path, int position, qreal offset = 0);

signals:
    void changed();
    void recentChanged();

private:
    QSettings m_settings;
};
