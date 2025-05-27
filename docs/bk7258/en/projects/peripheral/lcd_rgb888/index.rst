RGB888 LCD
========================

:link_to_translation:`zh_CN:[中文]`


1 Introduction
-------------------------------------

	The project is verified RGB888 interface display function,randoms color are displayed every second after power on

	the main differences of RGB888/RGB565/RGB666 project is whether the screen supports RGB565 or rgb666 or rgb888,
	the data source is not mandatory ,it can be RGB565, RGB888 or YUYV.

2 Code Path
-------------------------------------
	demo path:``./projects/peripheral/lcd_rgb888``


3.Code explanation
-------------------------------------
	compile commands:``make bk7258 PROJECT=peripheral/lcd_rgb888``
	
.. attention::

	LCD RGB565 or RGB666 or RGB888 is defined by lcd_device_t member "out_fmt", alse can config by API:
	src_fmt is the source data send to display, such as following is RGB888 send to LCD directly.

	LCD conifig by default:
	::

		const lcd_device_t lcd_device_st7701sn =
		{
			.id = LCD_DEVICE_ST7701SN,
			.name = "st7701sn",
			.type = LCD_TYPE_RGB565,    /**< lcd device hw interface *LCD_TYPE_RGB=LCD_TYPE_RGB565 */
			.ppi = PPI_480X854,
			.rgb = &lcd_rgb,
			.src_fmt = PIXEL_FMT_RGB888,   /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
			.out_fmt = PIXEL_FMT_RGB888, /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/
			.init = lcd_st7701sn_init,
			.lcd_off = NULL,
		};

	user can also use API to config:

	::
	
		lcd_hal_rgb_set_in_out_format(src_fmt, out_fmt);


RGB888 LCD config step
**********************************************

- step 1:open lcd

::

	media_app_lcd_pipeline_disp_open
	media_ipc_send

- step 2:malloc psram

::
	
	 frame_buffer_display_malloc

after psram malloc, should config frame:

::
	
	uint32_t size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 3;
	frame = frame_buffer_display_malloc(size);

	frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
	frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
	frame->fmt = PIXEL_FMT_RGB888;

like: frame->fmt = PIXEL_FMT_RGB888; is config source data RGB888 to display module;


- step 3:fill color

::
	 
	lcd_fill_rand_color

- step 4:display request

::

	lcd_display_frame_request


- step 5:cycle step2-4

