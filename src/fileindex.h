#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

#include "fuzzymodel.h"

class Settings;

// The Markdown files the fuzzy finder offers: recent files first, then every
// .md file under the search roots, found with fd (which skips hidden and
// git-ignored files and is fast enough to rescan each time the finder opens).
class FileIndex : public QObject {
    Q_OBJECT
    Q_PROPERTY(FuzzyModel *model READ model CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit FileIndex(Settings *settings, const QStringList &roots, QObject *parent = nullptr,
                       const QString &fd = {});
    ~FileIndex() override;

    FuzzyModel *model() { return &m_model; }
    bool scanning() const { return m_process.state() != QProcess::NotRunning; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();

    static QString label(const QString &path);

signals:
    void scanningChanged();
    void errorChanged();

private:
    void publish();
    void setError(const QString &error);

    Settings *m_settings;
    QStringList m_roots;
    QString m_fd;
    QStringList m_found;
    QProcess m_process;
    QString m_error;
    FuzzyModel m_model;
};
