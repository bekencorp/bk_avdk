RGB565 LCD
========================

:link_to_translation:`en:[English]`


1 功能概述
-------------------------------------
	本工程主要验证了RGB565接口的屏幕显示的功能，上电后会在屏幕上每隔一秒生成随机颜色进行显示。

	其中RGB888/RGB565/RGB666的工程主要区分的是屏幕支持的是RGB565，RGB666还是RGB888的接口，
	对于数据源提供的是RGB565/RGB888或YUYV不强制要求。

2 代码路径
-------------------------------------
	demo路径：``./projects/peripheral/lcd_rgb565``


3.代码讲解
-------------------------------------
	编译命令：``make bk7258 PROJECT=peripheral/lcd_rgb565``
	
.. attention::

	如何配置LCD的RGB565还是666或888接口是在每个屏幕的lcd_device_t 中的成员out_fmt中配置：

	其中src_fmt是指提供给display的数据是什么格式的数据，比如下面的配置就是将RGB565数据直接输出给LCD显示。


	LCD的默认配置如下：

	::

		const lcd_device_t lcd_device_st7701sn =
		{
			.id = LCD_DEVICE_ST7701SN,
			.name = "st7701sn",
			.type = LCD_TYPE_RGB565,    /**< lcd device hw interface *LCD_TYPE_RGB=LCD_TYPE_RGB565 */
			.ppi = PPI_480X854,
			.rgb = &lcd_rgb,
			.src_fmt = PIXEL_FMT_RGB565,   /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
			.out_fmt = PIXEL_FMT_RGB565, /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/
			.init = lcd_st7701sn_init,
			.lcd_off = NULL,
		};
		


RGB565 LCD 工程的配置步骤如下：
**********************************************

- 步骤1: 打开lcd, 并发送显示example的消息处理

::
	
	media_app_lcd_pipeline_disp_open
	media_ipc_send

- 步骤2：申请一帧psram作为显示

::
	
	 frame_buffer_display_malloc
	

申请psram成功后，需要对该frame进行参数配置,比如：

::

	uint32_t size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2;

	frame = frame_buffer_display_malloc(size);

	frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
	frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
	frame->fmt = PIXEL_FMT_RGB565;


- 步骤3：填充随机颜色
	 
::
	
	lcd_fill_rand_color

- 步骤4：显示该帧图片
	
::
	
	lcd_display_frame_request
	

- 步骤5：循环步骤2-4

