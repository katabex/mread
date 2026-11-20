#include "fileindex.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#include "settings.h"

FileIndex::FileIndex(Settings *settings, const QStringList &roots, QObject *parent, const QString &fd)
    : QObject(parent), m_settings(settings), m_roots(roots),
      m_fd(fd.isEmpty() ? QStandardPaths::findExecutable("fd") : fd) {
    connect(&m_process, &QProcess::stateChanged, this, &FileIndex::scanningChanged);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            setError(tr("Could not start fd: %1").arg(m_process.errorString()));
    });
    connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit)
            return;
        // fd exits 1 when some directories were unreadable but still lists
        // everything it could reach, so the output is used either way.
        m_found = QString::fromUtf8(m_process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        publish();
    });
    connect(m_settings, &Settings::recentChanged, this, &FileIndex::publish);
    publish();
}

FileIndex::~FileIndex() {
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString FileIndex::label(const QString &path) {
    const QString home = QDir::homePath();
    return path.startsWith(home + '/') ? "~" + path.mid(home.size()) : path;
}

void FileIndex::refresh() {
    if (m_fd.isEmpty()) {
        setError(tr("fd is not installed, so only recent files are listed"));
        return;
    }
    if (m_process.state() != QProcess::NotRunning)
        return;
    setError({});
    QStringList arguments{"--type", "f", "--extension", "md", "--extension", "markdown",
                          "--absolute-path", "--color", "never", "."};
    arguments += m_roots;
    m_process.start(m_fd, arguments);
}

void FileIndex::publish() {
    QStringList values;
    QSet<QString> seen;
    for (const auto &path : m_settings->recentFiles())
        if (!seen.contains(path) && QFileInfo::exists(path)) {
            seen.insert(path);
            values << path;
        }
    QStringList found = m_found;
    found.sort();
    for (const auto &path : std::as_const(found))
        if (!seen.contains(path)) {
            seen.insert(path);
            values << path;
        }
    QStringList labels;
    labels.reserve(values.size());
    for (const auto &path : std::as_const(values))
        labels << label(path);
    m_model.setItems(labels, values);
}

void FileIndex::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}
