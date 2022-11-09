QT += core gui widgets

CONFIG += c++17 utf8_source
QMAKE_MACOSX_DEPLOYMENT_TARGET=12.0

DESTDIR = $$PWD/bin

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

LIBS += $$PWD/libqrencode/lib/libqrencode.a
INCLUDEPATH += $$PWD/libqrencode/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
