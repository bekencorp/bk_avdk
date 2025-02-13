8080 LCD
========================

:link_to_translation:`en:[English]`


1 功能概述
-------------------------------------
	本工程主要验证了MCU接口的屏幕显示的功能，上电后会在屏幕上每隔一秒生成随机颜色进行显示。

2 代码路径
-------------------------------------
	demo路径：``./projects/peripheral/lcd_8080``


3.代码讲解
-------------------------------------
	编译命令：``make bk7258 PROJECT=peripheral/lcd_8080``
	
.. attention::

	如何配置LCD的RGB接口还是mcu 8080接口是在每个屏幕的lcd_device_t 中的type配置的


	LCD的默认配置如下：

	::

		const lcd_device_t lcd_device_st7796s =
		{
			.id = LCD_DEVICE_ST7796S,
			.name = "st7796s",
			.type = LCD_TYPE_MCU8080,
			.ppi = PPI_320X480,
			.mcu = &lcd_mcu,
			.init = lcd_st7796s_init,
			.lcd_off = st7796s_lcd_off,
		};

		
	该结构体中还有两个参数：
	::
	
		pixel_format_t src_fmt;  /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
		pixel_format_t out_fmt;   /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/


LCD MCU 工程的配置步骤如下：
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

