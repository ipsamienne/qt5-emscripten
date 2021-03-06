TARGET = qconnmanbearer

PLUGIN_TYPE = bearer
load(qt_plugin)

QT = core network-private dbus

HEADERS += qconnmanservice_linux_p.h \
           qofonoservice_linux_p.h \
           qconnmanengine.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h

SOURCES += main.cpp \
           qconnmanservice_linux.cpp \
           qofonoservice_linux.cpp \
           qconnmanengine.cpp \
           ../qnetworksession_impl.cpp

OTHER_FILES += connman.json

