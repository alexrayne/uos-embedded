TEMPLATE = app
CONFIG += console

SOURCES += \
    main.cpp

build_pass:CONFIG(release, debug|release) {
    DESTDIR = ../build_release
}else{
    DESTDIR = ../build
}

INCLUDEPATH += ../libuuload

win32 {
    INCLUDEPATH += ../include
    LIBS += -L../lib/winxp -lusb-1.0
}
unix {
    INCLUDEPATH += /usr/include/libusb-1.0
    LIBS += -L/usr/local/lib -lusb-1.0
}

