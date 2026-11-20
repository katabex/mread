QT += core gui quick testlib
CONFIG += c++17 testcase
TARGET = mread_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/document.h ../src/fileindex.h ../src/fuzzymodel.h ../src/settings.h ../src/theme.h
SOURCES += mread_tests.cpp ../src/document.cpp ../src/fileindex.cpp ../src/fuzzymodel.cpp \
           ../src/settings.cpp ../src/theme.cpp
