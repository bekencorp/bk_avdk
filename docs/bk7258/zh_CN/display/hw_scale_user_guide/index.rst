hw scale user guide
=====================================

1、功能概述
--------------------

	本文档主要介绍了硬件缩放的功能以及API使用方法。

2、参考代码
--------------------

	参考工程为 `DOORBELL工程 <../../projects_work/media/doorbell/index.html>`_

	参考组件代码在 ``components/image_codec_unit/src/pipeline/lcd_scale_pipeline.c``


	SCALE驱动代码参考 `hw_scale <../../api-reference/multi_media/bk_hw_scale.html>`_


3、缩放API使用
----------------------------
缩放的使用场景有两种，一种是整帧缩放，对应的解码为整帧解码，一种为按行解码，对应的为按行缩放；

3.1、整帧缩放
**************************

整帧缩放对应的代码只需要调用API

::

	bk_err_t lcd_scale_init(void)
	bk_err_t lcd_hw_scale(frame_buffer_t *src, frame_buffer_t *dst)

比如：

::

	lcd_info.scale_frame->width = (lcd_info.scale_ppi >> 16);
	lcd_info.scale_frame->height = (lcd_info.scale_ppi & 0xFFFF);
	lcd_info.scale_frame->fmt = frame->fmt;
	lcd_info.scale_frame->length = scale_length;
	lcd_hw_scale(frame, lcd_info.scale_frame);


3.1、按行缩放
**************************


按行缩放比较复杂，主要涉及的API有：

::

	bk_err_t scale_task_open(lcd_scale_t *lcd_scale)
	void bk_jdec_buffer_request_register(pipeline_module_t module, mux_request_callback_t cb, mux_reset_callback_t reset_cb)
	bk_err_t bk_scale_encode_request(pipeline_encode_request_t *request, mux_callback_t cb)



比如：

::

	bk_jdec_buffer_request_register(PIPELINE_MOD_SCALE, bk_scale_encode_request, bk_scale_reset_request);


	该函数将scale功能注册到解码模块，当行解码完成后，执行行缩放，其中 ``bk_scale_encode_request``是解码后缩放的请求，其两个参数分别为:

::

	 - pipeline_encode_request_t *request    //请求中携带的图片信息
	 - mux_callback_t cb                     // 注册到scale模块，缩放完成后，调用的回调函数


具体的使用方法详见 ```components/image_codec_unit/src/pipeline``