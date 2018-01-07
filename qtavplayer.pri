HEADERS += \
    $$PWD/avcodec.h \
    $$PWD/avoutput.h \
    $$PWD/avplayer.h

SOURCES += \
    $$PWD/avcodec.cpp \
    $$PWD/avoutput.cpp \
    $$PWD/avplayer.cpp

RESOURCES += \
    $$PWD/qtavplayer.qrc


win32{
    LIBS += -L$$PWD/libs/lib/win32/win32/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
}

INCLUDEPATH += $$PWD/libs/include
DEPENDPATH += $$PWD/libs/include
