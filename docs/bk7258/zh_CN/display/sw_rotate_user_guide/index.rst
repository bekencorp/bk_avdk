软件旋转
=====================================

:link_to_translation:`en:[English]`


1、功能概述
--------------------

本文档主要介绍了软件旋转的功能以及API使用。软件旋转可以实现YUV旋转和RGB565旋转。

2、参考代码
--------------------

	参考工程为 `DOORBELL工程 <../../projects_work/media/doorbell/index.html>`_

	硬件旋转的组件代码请参考 ``components/multimedia/lcd/lcd_rotate.c`` 和 ``components/image_codec_unit/src/pipeline/yuv_rotate_pipeline.c`` 

	软件旋转算法参考  `软件算法 <../../api-reference/multi_media/image_algorithm.html>`_

3、API使用说明
----------------------------
软件旋转的使用比较简单，主要调用一个接口既可以实现。
软件旋转主要是弥补硬件旋转不能实现的功能，比如将数据旋转为YUV，所以一般使用的接口为：

::

	yuyv_rotate_degree90_to_yuyv(unsigned char *yuyv, unsigned char *rotatedyuyv, int width, int height)
	yuyv_rotate_degree270_to_yuyv(unsigned char *yuyv, unsigned char *rotatedYuyv, int width, int height)
	yuyv_rotate_degree180_to_yuyv(unsigned char *yuyv, unsigned char *rotatedYuyv, int width, int height)
