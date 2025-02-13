LCD 常见问题
=================================


:link_to_translation:`en:[English]`


	本节主要描述调试和使用LCD过程中常遇到的问题和解决方法。
	异常可以分为display模块不出图，display模块输出图像但是屏幕没有刷图，出图了但是显示花屏，针对不同的异常，在此列出了一些常见的分析思路。

Q: display软件不打印帧率？
------------------------------------------

A：当模块正常刷屏，每2秒会有如下帧率显示的log打印,log示例如下：

::

	media_ui:I(43896008):dec:15[63298], lcd:58[4], lcd_fps:0[0], lvgl:0[0]

其中 dec为解码的帧率，lcd为模块硬件刷屏的物理帧率，lcd_fps为有效帧率一般和dec帧率对应，[]中的为历史积累帧率,该问题是指lcd一直为0。
客户遇到这种问题一般是软件问题，程序没有执行到display request，这是需要检查代码流程是否跑到显示这一步。



Q：display软件打印帧率,但是LCD不亮？
--------------------------------------------

A: 这种问题一般先检查硬件，从以下几个方面检查;

	 - LCD子板硬件接线是否接反
	 - 背光引脚是否正确，背光是否亮了
	 - 对于需要SPI初始化LCD的屏幕，检查SPI引脚接线是否正确，使用逻辑分析仪检查初始化命令是否正确。

	如果硬件没有问题，需要分析是否屏幕的初始化错误，导致LCD刷屏失败，这是最常见的问题，
	此时需要检查屏幕参数的配置或者需要联系屏厂检查初始化代码，用户可以从以下方面考虑：

	 - 该屏幕是否参与编译，是否使能
	 - 该屏幕时序配置范围是否和 lcd spec对应，参数是否超过或不满足其正常工作，即：

::

	static const lcd_rgb_t lcd_rgb =
	{
		.clk = LCD_30M,
		.data_out_clk_edge = NEGEDGE_OUTPUT,

		.hsync_pulse_width = 2,
		.vsync_pulse_width = 2,
		.hsync_back_porch = 46,
		.hsync_front_porch = 48,
		.vsync_back_porch = 24,
		.vsync_front_porch = 24,
	};

 - 屏幕设置的分辨率不能为0，如果为0,也会导致黑屏

Q： LCD显示花屏？
------------------------------------------

A:一般这个现象出现的原因有：

	 - RGB的某个引脚没有使能为RGB功能，会导致颜色大块异常
	 - 图像的分辨率和LCD的是否匹配，不匹配会导致显示一半，或切屏。
	 - 图像的数据格式是否为RGB数据，如果为YUV数据显示异常。
	 - 图象解码错误，错误的原因分析请参考 `JPEG decode <../../video_codec/jpeg_decoding_hw/index.html>`_ 

Q： LCD显示有撕裂？
----------------------------------------

A：如果屏幕支持TE功能，可以配置TE，或者软件上使用双buffer显示


Q： 如何设置RGB屏的RGB666/RGB888/RGB565接口？
----------------------------------------------

A：用户可以参考外设应用工程lcd_rgb565,lcd_rgb666,lcd_rgb888的工程配置

 `lcd_rgb565 <../../projects/peripheral/lcd_rgb565/index.html>`_ 

用户可以在LCD的配置源文件中修改默认的输入输出格式，比如：

::
	
	const lcd_device_t lcd_device_st7701sn =
	{
		.id = LCD_DEVICE_ST7701SN,
		.name = "st7701sn",
		.type = LCD_TYPE_RGB565,    /**< lcd device hw interface *LCD_TYPE_RGB=LCD_TYPE_RGB565 */
		.ppi = PPI_480X854,
		.rgb = &lcd_rgb,
		.src_fmt = PIXEL_FMT_YUYV,   /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
		.out_fmt = PIXEL_FMT_RGB888, /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/
		.init = lcd_st7701sn_init,
		.lcd_off = NULL,
	};

即设置Display模块的输入数据源是YUYV数据，屏幕的RGB接口是RGB888，display模块内部将YUYV数据转换为RGB888，输出给LCD。


如果是如下设置：

::
	
	.src_fmt = PIXEL_FMT_rgb565,   /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
	.out_fmt = PIXEL_FMT_RGB565, /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/

即设置Display模块的输入数据源是RGB565数据，屏幕的RGB接口是RGB565，display模块直接将RGB565数据输出给LCD。

也可以是如下设置:

::
	
	.src_fmt = PIXEL_FMT_RGB888,   /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
	.out_fmt = PIXEL_FMT_RGB666, /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/

即设置Display模块的输入数据源是RGB888数据，屏幕的RGB接口是RGB666，display模块内部将RGB888数据转换为RGB666，输出给LCD。



Q： 如何设置MCU屏的数据线是8线（D0-7）还是16线（D0-15）的RGB565/666/888接口？
-----------------------------------------------------------------------------

A:
	 - RGB565只支持8线输出：out_fmt = PIXEL_FMT_RGB565;
	 - RGB666只支持16线输出：out_fmt = PIXEL_FMT_RGB666
	 - RGB888即支持8线也支持16线输出:out_fmt = PIXEL_FMT_RGB888 或 out_fmt = PIXEL_FMT_RGB888_16BIT


用户可以在LCD的配置源文件中修改默认的输入输出格式，比如：

::
	
	const lcd_device_t lcd_device_st7796s =
	{
		.id = LCD_DEVICE_ST7796S,
		.name = "st7796s",
		.type = LCD_TYPE_MCU8080,
		.ppi = PPI_320X480,
		.src_fmt = PIXEL_FMT_YUYV;
		.out_fmt = PIXEL_FMT_RGB565;
		.mcu = &lcd_mcu,
		.init = lcd_st7796s_init,
		.lcd_off = st7796s_lcd_off,
	};

即设置Display模块的输入数据源是YUYV数据，MCU接口是8线RGB565，display模块内部将YUYV数据转换为RGB565，输出给MCU LCD。


这两个参数配置也可以通过API设置：

::
	
	 /*
	  * mcu input RGB565, rgb888. yuv dormat
	  * mcu output RGB565 rgb666, rgb888
	  */
	void lcd_hal_mcu_set_in_out_format(pixel_format_t in_fmt, pixel_format_t out_fmt)


Q： 选择RGB565或RGB666接口时，没有使用的IO是RGB的高位还是低位，可以作为普通IO使用吗？
--------------------------------------------------------------------------------------

A：RGB565或RGB666是高位有效，即选择RGB565时R0-R2, G0-G1,B0-B2这8个引脚可以作为普通GPIO使用。


Q： 解码一直失败, 打印“jpeg_decode_err_handler” 或“jpeg_decoder_isr int status = 0x200”
-----------------------------------------------------------------------------------------------------------------------------------

A: 解码失败的原因一般有：
	 - 图片头数据不完整或遭到破坏，无法解析出文件头,可以先保存图片，检查是否能正常显示。
	 - 摄像头出来的jpeg格式带DRI(define restart interval)段（字节码0xFFDD）的,工程默认配置不支持DRI的图片解码，需要将支持DRI的宏打开，即在工程目录<config/bk7258_cp1/bk7558_cp1/config>文件中添加CONFIG_JPEGDEC_HW_SUPPORT_DRI=y



