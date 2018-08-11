一个使用qt和ffmpeg编写的多媒体播放器，渲染采用opengl,目前只支持qml组件，目前功能有正常播放，拖动播放，播放控制等功能。
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
		
		
note : 如果需要替换ffmpeg库，直接替换libs/lib下面的库即可,最好是include和lib一起更换，保证是同一版本
