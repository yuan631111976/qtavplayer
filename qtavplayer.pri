HEADERS += \
    $$PWD/AVDefine.h \
    $$PWD/AVMediaCallback.h \
    $$PWD/AVOutput.h \
    $$PWD/AVPlayer.h \
    $$PWD/AVThread.h \
    $$PWD/AVDecoder.h \
    $$PWD/AVAudioEffect.h

SOURCES += \
    $$PWD/AVOutput.cpp \
    $$PWD/AVPlayer.cpp \
    $$PWD/AVThread.cpp \
    $$PWD/AVDecoder.cpp \
    $$PWD/AVAudioEffect.cpp

RESOURCES += \
    $$PWD/qtavplayer.qrc


win32{
    LIBS += -L$$PWD/libs/lib/win32/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
}

INCLUDEPATH += $$PWD/libs/include
DEPENDPATH += $$PWD/libs/include
