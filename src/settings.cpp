#include "settings.h"

#include <QFontDatabase>
#include <QStandardPaths>

namespace {

// Omarchy's terminal font when present, otherwise the system's fixed font.
QString defaultFamily() {
    static const QString family = [] {
        const QString omarchy = "JetBrainsMono Nerd Font";
        return QFontDatabase::hasFamily(omarchy) ? omarchy
                                                 : QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    return family;
}

constexpr int MaxRecent = 50;

} // namespace

Settings::Settings(const QString &file, QObject *parent)
    : QObject(parent),
      m_settings(file.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                      + "/mread/mread.conf"
                                : file,
                 QSettings::IniFormat) {}

QString Settings::bodyFont() const { return m_settings.value("bodyFont", defaultFamily()).toString(); }
QString Settings::codeFont() const { return m_settings.value("codeFont", defaultFamily()).toString(); }
int Settings::fontSize() const {
    return qBound(MinFontSize, m_settings.value("fontSize", DefaultFontSize).toInt(), MaxFontSize);
}
int Settings::columns() const {
    return qBound(MinColumns, m_settings.value("columns", DefaultColumns).toInt(), MaxColumns);
}
QStringList Settings::recentFiles() const { return m_settings.value("recentFiles").toStringList(); }
QString Settings::lastFile() const { return m_settings.value("lastFile").toString(); }
int Settings::lastPosition() const { return qMax(0, m_settings.value("lastPosition").toInt()); }
qreal Settings::lastOffset() const { return m_settings.value("lastOffset").toReal(); }

void Settings::setBodyFont(const QString &family) {
    if (family.isEmpty() || family == bodyFont())
        return;
    m_settings.setValue("bodyFont", family);
    emit changed();
}

void Settings::setCodeFont(const QString &family) {
    if (family.isEmpty() || family == codeFont())
        return;
    m_settings.setValue("codeFont", family);
    emit changed();
}

void Settings::setFontSize(int size) {
    size = qBound(MinFontSize, size, MaxFontSize);
    if (size == fontSize())
        return;
    m_settings.setValue("fontSize", size);
    emit changed();
}

void Settings::setColumns(int value) {
    value = qBound(MinColumns, value, MaxColumns);
    if (value == columns())
        return;
    m_settings.setValue("columns", value);
    emit changed();
}

void Settings::addRecent(const QString &path) {
    QStringList recent = recentFiles();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > MaxRecent)
        recent.removeLast();
    m_settings.setValue("recentFiles", recent);
    emit recentChanged();
}

void Settings::saveSession(const QString &path, int position, qreal offset) {
    if (path.isEmpty()) {
        m_settings.remove("lastFile");
        m_settings.remove("lastPosition");
        m_settings.remove("lastOffset");
        return;
    }
    m_settings.setValue("lastFile", path);
    m_settings.setValue("lastPosition", qMax(0, position));
    m_settings.setValue("lastOffset", qIsFinite(offset) ? offset : 0.0);
}
