HEADERS += \
    $$PWD/AVDefine.h \
    $$PWD/AVMediaCallback.h \
    $$PWD/AVMediaPlayer.h \
    $$PWD/AVOutput.h \
    $$PWD/AVPlayer.h \
    $$PWD/AVPlayerCallback.h \
    $$PWD/AVThread.h \
    $$PWD/AVDecoder.h

SOURCES += \
    $$PWD/AVMediaPlayer.cpp \
    $$PWD/AVOutput.cpp \
    $$PWD/AVPlayer.cpp \
    $$PWD/AVThread.cpp \
    $$PWD/AVDecoder.cpp

RESOURCES += \
    $$PWD/qtavplayer.qrc


win32{
    LIBS += -L$$PWD/libs/lib/win32/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
}

INCLUDEPATH += $$PWD/libs/include
DEPENDPATH += $$PWD/libs/include
