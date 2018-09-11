一个使用qt和ffmpeg编写的多媒体播放器，渲染采用opengl,目前只支持qml组件，目前功能有正常播放，拖动播放，播放控制等功能。不需要提前编译，只需要引入自己的项目即可以使用.
使用方法:
	1:在pro文件里面进行配置:
		导入qtavplayer项目：
		include ($$PWD/qtavplayer/qtavplayer.pri)
		
		添加多媒体库用于播放音频
		QT += multimedia
		
		添加头文件
		INCLUDEPATH += $$PWD/qtavplayer
		DEPENDPATH += $$PWD/qtavplayer
		
	2.在main方法里面注册qml组件
		例：
		qmlRegisterType<AVDefine>("qtavplayer", 1, 0, "AVDefine");
		qmlRegisterType<AVPlayer>("qtavplayer", 1, 0, "AVPlayer");
		qmlRegisterType<AVOutput>("qtavplayer", 1, 0, "AVOutput");
	3.在QML里面使用此组件
		例:
		import qtavplayer 1.0
		
		AVOutput{
			source: avplayer
		}
		
		AVOutput{ //可以同时在多个窗口上播放一个视频
			source: avplayer
		}
		AVPlayer{
			id : avplayer
			source : "url"
		}
		
		
note : 如果需要替换ffmpeg库，直接替换libs/lib下面的库和libs/include的头文件即可 ,windows的动态库放在 libs/lib/win32/下面

目前支持以下格式直接使用OPENGL渲染，其它格式会转换成YUV420P进行渲染（后期会逐渐增加可直接渲染的格式）:
AV_PIX_FMT_YUV420P
AV_PIX_FMT_YUVJ420P
AV_PIX_FMT_YUV422P
AV_PIX_FMT_YUVJ422P
AV_PIX_FMT_YUV444P
AV_PIX_FMT_YUVJ444P
AV_PIX_FMT_GRAY8
AV_PIX_FMT_UYVY422
AV_PIX_FMT_YUYV422
AV_PIX_FMT_YUV420P10LE
AV_PIX_FMT_BGR24
AV_PIX_FMT_RGB24
AV_PIX_FMT_YUV410P
AV_PIX_FMT_YUV411P
AV_PIX_FMT_MONOWHITE
AV_PIX_FMT_MONOBLACK
AV_PIX_FMT_PAL8
AV_PIX_FMT_UYYVYY411
AV_PIX_FMT_BGR8
AV_PIX_FMT_RGB8
AV_PIX_FMT_NV12
AV_PIX_FMT_NV21
AV_PIX_FMT_ARGB
AV_PIX_FMT_RGBA
AV_PIX_FMT_ABGR
AV_PIX_FMT_BGRA
AV_PIX_FMT_GRAY16LE
AV_PIX_FMT_YUV440P
AV_PIX_FMT_YUVJ440P
AV_PIX_FMT_YUVA420P
AV_PIX_FMT_YUV420P16LE
AV_PIX_FMT_YUV422P16LE
AV_PIX_FMT_YUV444P16LE
AV_PIX_FMT_YUVA420P16LE
AV_PIX_FMT_YUVA422P16LE
AV_PIX_FMT_YUVA444P16LE
AV_PIX_FMT_YUV444P10LE
AV_PIX_FMT_RGB565LE
AV_PIX_FMT_RGB555LE
AV_PIX_FMT_BGR565LE
AV_PIX_FMT_BGR555LE
AV_PIX_FMT_RGB444LE
AV_PIX_FMT_BGR444LE
AV_PIX_FMT_YUV422P10LE
AV_PIX_FMT_YUVA420P10LE
AV_PIX_FMT_YUVA422P10LE
AV_PIX_FMT_YUVA444P10LE
AV_PIX_FMT_NV16
AV_PIX_FMT_NV20LE

交流QQ：631111976