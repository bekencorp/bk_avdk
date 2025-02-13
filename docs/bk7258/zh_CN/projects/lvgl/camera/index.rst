lvgl和camera图像切换显示工程
=================================

:link_to_translation:`en:[English]`


1. 简介
--------------------

本工程是基于lvgl实现的一个二维码生成应用ui，并支持动态切换显示camera图像视频流和lvgl ui。


1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	* 硬件配置：
		* 核心板，**BK7258_QFN88_9X9_V3.2**
		* 屏幕转接板，**BEKEN_LCD_V3**
		* PSRAM 8M/16M
		* 屏幕，**ST7701SN**，480 * 854 RGB LCD


1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

	工程路径: ``<bk_avdk源代码路径>/project/lvgl/camera``


2. 框架图
---------------------------------

2.1 软件模块架构图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

软件模块架构图见：`图形用户界面 <../../../gui/lvgl/index.html>`_ 中的lvgl framework图。


2.2 代码模块关系图
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

LVGL的接口都定义在 **lv_vendor.h** 和 **lvgl.h** 中。


3. 演示说明
---------------------------------

	烧录上电后lvgl ui会自动启动运行，LCD默认会显示一个二维码静态页面，然后可按照以下方法进行切换测试：

  - 手机上安装IoT软件，安装好后点击该软件进行主页面；
  - 使用IoT软件点击右上角添加 ``BK7258_DL_04`` 设备，点击开始添加，点击 ``选择Wi-Fi`` 连接可连接互联网的网络并点击下一步；
  - 选择自己的蓝牙设备，可在上电开机的log中查看，也可输入 ``mac`` 命令查看对应的设备MAC地址，点击并等待设备连接网络进度至100%；
  - 手机自动跳转到摄像头预览页面，发送 ``lvcam_open`` 命令可在LCD屏上显示camera图像，输入 ``lvcam_close`` 命令可在LCD屏上显示LVGL图像，依次进行不断切换显示。

.. note::
	该工程可不使用手机IoT软件进行测试，直接输入 ``media uvc open 864X480`` 命令打开摄像头，接着再输入对应的切换命令。


4. 代码讲解
---------------------------------

::

    void lvcamera_main_init(void)
    {
        bk_err_t ret;

        cli_lvcamera_init();

        //只打开LCD显示功能，不打开旋转和解码等功能
        ret = media_app_lcd_pipeline_disp_open((lcd_open_t *)&lcd_open);
        if (ret != BK_OK) {
            os_printf("media_app_lcd_pipeline_open failed\r\n");
            return;
        }

        //打开LVGL并启动UI绘制
        ret = media_app_lvgl_open((lcd_open_t *)&lcd_open);
        if (ret != BK_OK) {
            os_printf("media_app_lvgl_draw failed\r\n");
            return;
        }
    }

    //切换到camera图像显示
    void lvcamera_open(void)
    {
        os_printf("%s\r\n", __func__);
        bk_err_t ret;

        if (lvcam_is_open) {
            os_printf("lvcam is already open\r\n");
            return;
        }

        if (lcd_jdec_is_first_open) {
            //设置旋转90度
            media_app_pipline_set_rotate(ROTATE_90);

            //打开camera视频流的解码和旋转等功能
            ret = media_app_lcd_pipeline_jdec_open();
            if (ret != BK_OK) {
                os_printf("media_app_lcd_pipeline_jdec_open failed\r\n");
                return;
            }
            lcd_jdec_is_first_open = false;
        }

        ret = media_app_lvcam_lvgl_close();
        if (ret != BK_OK) {
            os_printf("media_app_lvgl_close failed\r\n");
            return;
        }

        lvcam_is_open = true;
    }

    //切换到lvgl ui图像显示
    void lvcamera_close(void)
    {
        os_printf("%s\r\n", __func__);
        bk_err_t ret;

        if (!lvcam_is_open) {
            os_printf("lvcam has not been opened, please input the \"lvcam open \" command\r\n");
            return;
        }

        ret = media_app_lvcam_lvgl_open((lcd_open_t *)&lcd_open);
        if (ret != BK_OK) {
            os_printf("media_app_lvgl_open failed\r\n");
            return;
        }

        lvcam_is_open = false;
    }

.. note::
	在输入 ``lvcam open`` 命令后，视频流的所有功能都将打开，且切换到lvgl显示时也不会关闭camera和lcd，变量 ``lvgl_disp_enable`` 主要用来控制显示的流数据。

