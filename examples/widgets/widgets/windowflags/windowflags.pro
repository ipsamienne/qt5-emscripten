HEADERS       = controllerwindow.h \
                previewwindow.h
SOURCES       = controllerwindow.cpp \
                previewwindow.cpp \
                main.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/widgets/windowflags
INSTALLS += target

QT += widgets

simulator: warning(This example might not fully work on Simulator platform)
