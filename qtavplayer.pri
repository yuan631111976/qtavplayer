QT += multimedia

INCLUDEPATH += $$PWD/libs/include
DEPENDPATH += $$PWD/libs/include

HEADERS += \
    $$PWD/avcodec.h \
    $$PWD/avoutput.h \
    $$PWD/avplayer.h \
    $$PWD/U_YuvManager.h

SOURCES += \
    $$PWD/avcodec.cpp \
    $$PWD/avoutput.cpp \
    $$PWD/avplayer.cpp \
    $$PWD/U_YuvManager.cpp

RESOURCES += \
    $$PWD/qtavplayer.qrc


win32{
    LIBS += -L$$PWD/libs/lib/win32/win32/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
}

android{
    contains(ANDROID_TARGET_ARCH,armeabi-v7a) {
        ANDROID_EXTRA_LIBS = \
            $$PWD/libs/lib/android/armeabi-v7a/libavcodec.so \
            $$PWD/libs/lib/android/armeabi-v7a/libavfilter.so \
            $$PWD/libs/lib/android/armeabi-v7a/libavformat.so \
            $$PWD/libs/lib/android/armeabi-v7a/libavutil.so \
            $$PWD/libs/lib/android/armeabi-v7a/libswresample.so \
            $$PWD/libs/lib/android/armeabi-v7a/libswscale.so

        LIBS += -L$$PWD/libs/lib/android/armeabi-v7a/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
    }
    contains(ANDROID_TARGET_ARCH,x86) {
        LIBS += -L$$PWD/libs/lib/android/x86/ -lavcodec -lavfilter -lavformat -lavutil -lswresample -lswscale
    }
}
