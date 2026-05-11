QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    analyzer.cpp \
    cgen.cpp \
    code.cpp \
    main.cpp \
    mainwindow.cpp \
    parser.cpp \
    scanner.cpp \
    symtab.cpp \
    util.cpp

HEADERS += \
    analyzer.h \
    cgen.h \
    code.h \
    globals.h \
    mainwindow.h \
    parser.h \
    scanner.h \
    symtab.h \
    util.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    TinyCompilerGUI_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
