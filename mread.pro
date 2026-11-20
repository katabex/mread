QT += core gui qml quick quickcontrols2

CONFIG += c++17 release
TARGET = mread
TEMPLATE = app

HEADERS += \
    src/document.h \
    src/fileindex.h \
    src/fuzzymodel.h \
    src/markdownview.h \
    src/settings.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/document.cpp \
    src/fileindex.cpp \
    src/fuzzymodel.cpp \
    src/markdownview.cpp \
    src/settings.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
