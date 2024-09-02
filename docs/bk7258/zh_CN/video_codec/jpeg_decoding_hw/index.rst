JPEG硬解码
=================================

:link_to_translation:`en:[English]`


1、功能概述
--------------------

本文档主要介绍了硬件解码的API使用方法。

用户必须知道硬件解码的限制以及注意事项如下：

.. attention::

	1) 解码支持输入分辨率的列必须是16的整数倍，行必须是8的整数倍。

	2) 解码的输出格式只支持下面四种YUV格式，默认为YUYV

	 - 格式1：y0uy1v(高位<-->低位)（byte3，byte2，byte1，byte0）
	 
	 - 格式2：vuy1y0(高位<-->低位)（byte3，byte2，byte1，byte0）
	 
	 - 格式3：vy1uy0(高位<-->低位)（byte3，byte2，byte1，byte0）
	 
	 - 格式4：y0y1uv(高位<-->低位)（byte3，byte2，byte1，byte0）
	 

	3)解码输入的jpeg只支持YUV422压缩编码的jpeg/jpg，不支持YUV420编码的图片解码；

2、参考代码
--------------------
	硬解码的驱动API请参考 `API Reference : <../../api-reference/multi_media/bk_jpegdec_hw.html>`_

	硬解码的组件代码请参考 ``components/multimedia/lcd/lcd_decode.c`` 和 ``components/image_codec_unit/src/pipeline/jpeg_decode_pipeline.c`` 

3、JPEG硬解码流程
----------------------

JPEG硬解码主要流程如下：

1) 首先有yuv422编码的jpeg数据；

2) 初始化解码模块，并注册中断函数, 解码支持整帧解码和按行解码，其中按行解码时，行数有以下选择: 

::

	typedef enum _line_num {
		LINE_8 = 0,
		LINE_16,
		LINE_24,
		LINE_32,
		LINE_40,
		LINE_48,

	}line_num_t;

即当行数为8/16/..时，行解码结束，进入行中断。

初始化代码：

::

	bk_jpeg_dec_driver_init();
	bk_jpeg_dec_isr_register(DEC_ERR, jpeg_decode_err_handler);
	if (jdec_config->jdec_type == JPEGDEC_BY_LINE)
	{
		if (PIPELINE_DECODE_LINE == 16)
		{
			bk_jpeg_dec_line_num_set(LINE_16);
		}
		else
		{
			LOGE("%s, to config decode line \n", __func__);
		}
		bk_jpeg_dec_isr_register(DEC_EVERY_LINE_INT, jpeg_decode_line_complete_handler);
	}
	else
	{
		bk_jpeg_dec_isr_register(DEC_END_OF_FRAME, jpeg_decode_frame_complete_handler);
	}

2) 然后调用API执行硬件解码:

::

	bk_err_t bk_jpeg_dec_hw_start(uint32_t length, unsigned char *input_buf, unsigned char * output_buf)


3) 解码完成后会进入解码完成中断，用户可以在解码完成中断中获取图片的像素

::

	/** jpeg dec isr return value */
	typedef struct
	{
		bool ok;			/**< jpeg decoder success */
		uint16_t pixel_x;  /**< jpeg x pixel */
		uint16_t pixel_y;  /**< jpeg y pixel */
		uint32_t size;     /**< jpeg size */
	} jpeg_dec_res_t;


5) 当解码进入 ``jpeg_decode_err_handler`` 中断，说明解码错误，可能的原因是：

 - jpeg头解析错误，比如量化表或霍夫曼表解析错误
 - 图片数据不完整
 - 解析出来的头和数据不一致，图片数据找到破坏
 - 其他原因

6) 如果硬件解码失败，也可以调用API获取当前图片的编码格式：

::

	yuv_enc_fmt_t bk_get_original_jpeg_encode_data_format(uint8_t *src_buf, uint32_t length)


