QT += core gui widgets

CONFIG += staticlib
CONFIG += c++17 utf8_source
QMAKE_MACOSX_DEPLOYMENT_TARGET=12.0
DESTDIR = $$PWD/bin

SOURCES += \
    main.cpp \
    mainwindow.cpp\


HEADERS += \
        mainwindow.h\
        imgconvertors.h

FORMS += \
    mainwindow.ui
OPENCV_WIN32_LIB_PATH=$$PWD/opencv/x64/mingw/lib
OPENCV_ELSE_LIB_PATH=$$PWD/opencv/lib
win32:LIBS += $$OPENCV_WIN32_LIB_PATH/libopencv_core460.dll.a $$OPENCV_WIN32_LIB_PATH/libopencv_imgproc460.dll.a $$OPENCV_WIN32_LIB_PATH/libopencv_wechat_qrcode460.dll.a
else:LIBS += -L$$OPENCV_ELSE_LIB_PATH -lopencv_core -lopencv_imgproc -lopencv_wechat_qrcode
INCLUDEPATH += $$PWD/opencv/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
