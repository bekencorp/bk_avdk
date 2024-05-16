Camera Debug Info
=================================

:link_to_translation:`en:[English]`

1 功能概述
--------------------
	本工程主要功能是在硬件接入uvc/dvp摄像头之后，可以从串口获得MJPEG解码后每次的数据信息，如解码之后的数据格式，是否旋转等

2 代码路径
-------------------------------------
	demo路径：``./projects/peripheral/uvc`` 或 ``./projects/peripheral/dvp``

3 编译命令
-------------------------------------
	编译命令：``make bk7258 PROJECT=peripheral/uvc`` 或 ``make bk7258 PROJECT=peripheral/dvp``

4 演示介绍
-------------------------------------
	根据使用的摄像头类型选择对应的编译命令，将开发板摄像头连接并下载程序，打开串口按下复位键即可看见信息打印。信息主要有摄像头获取到的MJPEG数据以及MJPEG解码之后输出的数据信息。

  	摄像头获取的MJPEG数据信息有摄像头类型（camera_type），读取到的frame的frame_id，frme_length,以及frame地址。
	MJPEG解码之后的数据的ppi，旋转的角度，输出的数据格式以及大小端。

	.. figure:: ../../../_static/camera_mjpeg_decode_info.png
		:align: center
		:alt: camera_mjpeg_decode info
		:figclass: align-center

		Figure 1. 串口输出信息