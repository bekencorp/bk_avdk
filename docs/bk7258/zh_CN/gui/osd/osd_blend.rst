OSD视频叠加
=================================


:link_to_translation:`en:[English]`


1、功能概述
--------------------

	OSD(on screen display)，视频叠加显示, 在视频或图片上叠加字符和图标显示。
	其中，字符叠加使用的是CPU运算实现，图标叠加可以使用CPU，也可以使用DMA2D的方式实现。
	本文档主要介绍了如何调用API实现图片和汉字的融合。

2、参考代码
--------------------

	OSD的融合资源生成示例在doorbell工程：``projects/media/doorbell/main/src/assets``

	DMA2D用户使用参考 `DMA2D用户手册参考 <../../display/dma2d_user_guide/dma2d_user_guide.html>`_

	融合的驱动代码在 ``components/bk_draw_blend``


3、融合资源生成
---------------------

3.1 字库软件生成字库资源
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


字库融合需要先生成字库，然后融合到图片上，关于字库生成的文档请参考 `字库生成 <font_generate.html>`_


生成的字库资源，比如font_digit_Roboto53需要参照bk_blend_t结构体定义要显示的标签，示例如下：

::

	GUI_CONST_STORAGE bk_blend_t font_clock = 
	{
		.version = 0,
		.blend_type = BLEND_TYPE_FONT,
		.name = "clock",
		.width = CLOCK_LOGO_W,
		.height = CLOCK_LOGO_H,
		.xpos = 0,
		.ypos = 0,
		.font = 
		{
			.font_digit_type = font_digit_Roboto53,
			.color = FONT_COLOR,
		},
	};

3.2 ffmpeg生成图片资源
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


 -  将png转换为rgba8888的数据，该转换可以实现保持png的透明度，适合融合背景是透明的图标。判断图片背景是否透明，可以用photoshop查看背景是否是镂空。

 - png转换工具路径： ``components/multimedia/tools/ffmpeg_bat/png2argb`` 将所有的png图片访在该路径下，双击run.bat.

 .. figure:: ../../../../common/_static/png2rgba.png
    :align: center
    :alt: RealtimeVideo_app
    :figclass: align-center

    Figure. png to rgba8888.rgb

 - 或将jpg转换为rgb565(大端)的数据，该转换过程中将所有不透明的jpg像素默认转换为不透明，适合对背景框没有要求的融合。
 
 - 工具路径： ``components/multimedia/tools/ffmpeg_bat/jpeg2rgb565`` 将所有的jpg图片访在该路径下，双击run.bat.

 .. figure:: ../../../../common/_static/jpg2rgb565.png
    :align: center
    :alt: RealtimeVideo_app
    :figclass: align-center

    Figure. png to rgb565le.rgb

 - 使用 "HxD"或其他工具将rgb数据转成const数组并保存到flash中。

生成的图片资源需要参照bk_blend_t结构体定义要显示的标签，示例如下：

::

	GUI_CONST_STORAGE bk_blend_t img_wifi_rssi0 =
	{
		.version = 0,
		.blend_type = BLEND_TYPE_IMAGE,
		.name = "wifi",
		.width = WIFI_WIDTH,
		.height = WIFI_HEIGHT,
		.xpos = WIFI_XPOS,
		.ypos = WIFI_YPOS,
		.image = 
		{
			.format = ARGB8888,
			.data_len = WIFI_WIDTH * WIFI_HEIGHT *4,
			.data = wifi_0_argb8888
		},
	};



3.3 使用UI工具生成字库和图片资源
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

使用UI工具生成资源集成了3.1和3.2章节的功能，是用户主要使用的生成资源的方式。


UI工具的使用请参考 ` https://docs.bekencorp.com/arminodoc/bk_app/gui_designer/zh_CN/latest/index.html`_


使用UI生成的资源不仅输出的图片或字库资源，还有标签的结构体定义，不需要用户再添加。


3.4 资源字典
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,


所有融合资源定义在 ``project/doorbell/src/assets/blend_dsc.c`` blend_assets数组中:

::

	const blend_info_t blend_assets[] =
	{
		{.name = "clock", .addr = &font_clock, .content = "12:30"},
		{.name = "date",  .addr = &font_dates, .content = "2025/1/2 周四"},
		{.name = "ver",   .addr = &font_ver, .content = "v 1.0.0"},
		{.name = "wifi", .addr = &img_wifi_rssi0, .content = "wifi0"},
		{.name = "wifi", .addr = &img_wifi_rssi1, .content = "wifi1"},
		{.name = "wifi", .addr = &img_wifi_rssi2, .content = "wifi2"},
		{.name = "wifi", .addr = &img_wifi_rssi3, .content = "wifi3"},
		{.name = "wifi", .addr = &img_wifi_rssi4, .content = "wifi4"},
		{.name = "battery", .addr = &img_battery1, .content = "battery1"},
		{.name = "weather",.addr = &img_cloudy_to_sunny, .content = "cloudy_to_sunny"},
		{.addr = NULL},
	};

其中blend_info_t的定义如下：

::

	typedef struct 
	{
		char name[MAX_BLEND_NAME_LEN];
		const bk_blend_t *addr;            //the pointer, pointer to the struct
		char content[MAX_BLEND_CONTENT_LEN];
	}blend_info_t;


该资源字典可以通过UI工具生成。



3、融合API
-------------------------------


1. 单个字库标签融合调用的API接口为：

::

	/**
	 * @brief  draw font by cpu
	 * @param  blend background layer frame
	 * @param  blend panel lcd width, to calculate postion in panel by (x, y) pos
	 * @param  blend panel lcd heighe, to calculate postion in panel by (x, y) pos
	 * @param  struct addr of image type blend_info_t
	 * @example:
	 *          extern GUI_CONST_STORAGE bk_blend_t font_clock;
	 *          blend_info_t blend_info = {.name = clock, .addr = &font_clock, .content = "12:30"};
	 *          bk_display_blend_font_handle(frame, lcd_w, lcd_h, &blend_info);
	 *
	 * @return 
	 *     - BK_OK: no error
	 *     - BK_FAIL:not find blend image
	 */
	bk_err_t bk_display_blend_font_handle(frame_buffer_t *frame, uint16_t lcd_width, uint16_t lcd_height, const blend_info_t *font_info)



2. 单个图片融合调用的API为：

::

	/**
	 * @brief  blend icon of ARGB888 image by cpu or hardware dma2d
	 * @param  blend background layer frame
	 * @param  blend panel lcd width, to calculate postion in panel by (x, y) pos
	 * @param  blend panel lcd heighe, to calculate postion in panel by (x, y) pos
	 * @param  struct addr of image type bk_blend_t
	 * @example:
	 *          extern GUI_CONST_STORAGE bk_blend_t img_wifi_rssi1;
	 *          bk_display_blend_img_handle(frame, lcd_w, lcd_h, &img_wifi_rssi1);
	 *
	 * @return 
	 *     - BK_OK: no error
	 *     - BK_FAIL:not find blend image
	 */
	bk_err_t bk_display_blend_img_handle(frame_buffer_t *frame, uint16_t lcd_width, uint16_t lcd_height, const bk_blend_t *img_info);


3. 融合数组调用的API为：

融合数组可以通过UI工具生成当前界面所需融合的数组列表：

::

	const blend_info_t blend_info[] =
	{
		{.name = "clock", .addr = &font_clock, .content = "12:30"},
		{.name = "date",  .addr = &font_dates, .content = "2025/1/2 周四"},
		{.name = "ver",   .addr = &font_ver, .content = "v 1.0.0"},
		{.name = "wifi", .addr = &img_wifi_rssi0, .content = "wifi0"},
		{.name = "battery", .addr = &img_battery1, .content = "battery1"},
		{.name = "weather",.addr = &img_cloudy_to_sunny, .content = "cloudy_to_sunny"},
		{.addr = NULL},
	};


也可以通过调用media_app_lcd_blend() API, 通过查找资源字典中的资源组成一个动态数组g_dyn_array;

::

	os_strcpy((char *)blend.name,  "clock");
	os_strcpy((char *)blend.content, "12:00");
	ret = media_app_lcd_blend(&blend);

	os_strcpy((char *)blend.name,  "wifi");
	os_strcpy((char *)blend.content, "wifi0");
	ret = media_app_lcd_blend(&blend);


然后执行bk_display_blend_handle接口实现数组的融合.：

::

	blend_info_t *info = &g_dyn_array.entry[0];
	bk_display_blend_handle((frame_buffer_t *)msg.param, lcd_disp_config->lcd_widthlcd_disp_config->lcd_height, info);



5. 注意事项
---------------------------------

.. attention::

	融合需要先将背景图片拷贝到sram或psram中，融合后再拷贝回去，拷贝的大小是根据 bk_blend_t 中的成员width和height决定。图标的尺寸如何确定请参考字库生成文档 `font generate <../osd/font_generate.html>`_
	代码中默认使用psram内存，如果使用sram,需要将BLEND_MALLOC_SRAM宏打开，LCD_BLEND_MALLOC_SIZE为sram申请的大小， 如果图标所需的内存超过该值，代码自动转换为psram申请。

::

	#define BLEND_MALLOC_SRAM           1
	#define LCD_BLEND_MALLOC_SIZE      (1024 * 15)

	图标的尺寸如何确定请参考字库生成文档 `font generate <../osd/font_generate.html>`_


.. attention::

	对于DMA2D硬件融合，是硬件直接将前景图片融合到背景的指定坐标点，不需要申请额外的内存，背景数据格式可以为YUV/RGB565/RGB888，前景为ARGB8888，还可以设置前景的透明度。所以，
	对于图标的融合使用优先选择硬件DMA2D，但是由于DMA2D在doorbell工程中（按行解码旋转模式）作为拷贝数据已经使用，所以工程中默认使用CPU融合。