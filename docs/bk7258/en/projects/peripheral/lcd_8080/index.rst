8080 LCD
========================

:link_to_translation:`zh_CN:[中文]`


1 Introduction
-------------------------------------
	The project is verified mcu interface display function,randoms color are displayed every second after power on

2 Code Path
-------------------------------------
	demo path:``./projects/peripheral/lcd_8080``


3.Code explanation
-------------------------------------
	compile commands:``make bk7258 PROJECT=peripheral/lcd_8080``
	
.. attention::

	how to config LCD RGB interface or MCU 8080 is configed by cd_device_t member "type"


	LCD conifig by default:

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

		
	the another params:
	
	::
	
		pixel_format_t src_fmt;  /**< source data format: input to display module data format(rgb565/rgb888/yuv)*/
		pixel_format_t out_fmt;   /**< display module output data format(rgb565/rgb666/rgb888), input to lcd device,*/

	is the same function as to the following API:

	::
	
		lcd_hal_rgb_set_in_out_format(src_fmt, out_fmt);

LCD MCU config step
**********************************************

- step 1: open lcd

::
	
	media_app_lcd_pipeline_disp_open
	media_app_lcd_example_display

- step 2:malloc psram

::
	
	 frame_buffer_display_malloc
	

after psram malloc, should config frame:

::

	uint32_t size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2;

	frame = frame_buffer_display_malloc(size);

	frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
	frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
	frame->fmt = PIXEL_FMT_RGB565;


- step 3:fill color
	 
::
	
	lcd_fill_rand_color

- step 4:display request
	
::
	
	lcd_display_frame_request
	

- step 5:cycle step2-4

