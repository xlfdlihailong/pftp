TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Ftp.c \
    Socket.c \
    pftp.cpp \
    ../../c/clib/clib.c \
    ../plib/plib.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    Ftp.h \
    Socket.h \
    pftp.h \
    ../../c/clib/clib.h \
    ../plib/plib.h

LIBS+=-lpthread
