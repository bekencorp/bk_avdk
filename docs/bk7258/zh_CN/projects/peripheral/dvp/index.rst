DVP
=================================


:link_to_translation:`en:[English]`

1. 简介
---------------------------------

本工程是DVP默认调试工程，可以用来测试插入的DVP工作是否正常，默认使用16M PSRAM。

1.1 规格
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    请参考 `规格 <../../media/doorviewer/index.html#id2>`_

1.2 路径
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk源代码路径>/projects/peripheral/dvp

2. 框架图
---------------------------------

    请参考 `框架图 <../../media/doorviewer/index.html#id4>`_

3. 配置
---------------------------------

3.1 蓝牙与多媒体内存复用
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    使用此工程，只是单纯验证dvp，故内存不需要与蓝牙共用。

    +-------------------------------------+---------------+-------------------------------------+
    |          marco                      |     value     |           implication               |
    +-------------------------------------+---------------+-------------------------------------+
    | CONFIG_BT_REUSE_MEDIA_MEMORY        |       N       | 多媒体和蓝牙共用一块sram（分时复用）|
    +-------------------------------------+---------------+-------------------------------------+

3.1.1 卸载蓝牙
.................................

::

    #ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
    #if CONFIG_BLUETOOTH
        bk_bluetooth_deinit();
    #endif
    #endif

3.1.2 初始化蓝牙
.................................

::

    bk_bluetooth_init();

3.2 区别
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    dvp与doorviewer的区别在于：
        * dvp默认上电启动dvp，并通过软件解码器，分析输出的JPEG图像原始数据格式YUV422/YUV420，和输出分辨率。其他外设不参与，不支持图传，LCD显示，语音传输等。
        * doorviewer是一个完整的门锁方案。

4. 演示说明
---------------------------------

    默认上电自启动，不需要额外操作，会打印如下log。

::

    (7598):##MJPEG:camera_type:1(1:dvp 2:uvc) frame_id:178, length:26163, frame_addr:0x6011f7a0
    video_co:I(7598):4:2:2, YUV:
    (7598):##DECODE:pixel_x:640, pixel_y:480
    (7600):rotate_angle:0(0:0 1:90 2:180 3:270)
    (7600):byte_order:0(0:little endian 1:big endian)
    (7600):out_fmt:YUYV

5. 代码讲解
---------------------------------

    请参考 `代码讲解 <../../media/doorviewer/index.html#id13>`_

6. 新增适配
---------------------------------

    当适配一个新的摄像头，调试步骤如下：

    步骤一：参考其他外设的配置方式：”./components/bk_peripheral/src/dvp/dvp_gc2145.c”，添加新的dvp_xx.c文件添加到相应的文件夹下，
    并将新增的外设添加到”dvp_sensor_devices.c”中，修改: ”./components/bk_peripheral“目录下的CMakeList.txt文件，将新增的dvp_xx.c文件加入到编译中。

    步骤二：在dvp_xx.c中明确I2C读写地址，明确使用的硬件I2C ID，BK7258默认使用的是I2C1，对应的GPIO为GPIO0和GPIO1。DVP使用的I2C ID通过宏配置。

    +------------------------------+---------------+-------------------------------------+
    |             marco            |     value     |           implication               |
    +------------------------------+---------------+-------------------------------------+
    | CONFIG_DVP_CAMERA_I2C_ID     |       1       |           使用的I2C序号             |
    +------------------------------+---------------+-------------------------------------+

    步骤三：在dvp_xx.c中明确摄像头的CHIP_ID。

    步骤四：在dvp_xx.c中明确结构体：“dvp_sensor_config_t”的成员。注意，所配置的参数需要与摄像头的配置一致；比如CLK=MCLK_24，表明sensor的输入时钟为24MHz，
    而不是其他值，否则与预期的帧率不一致；比如fmt = PIXEL_FMT_YUYV，表明sensor出图的格式是YUYV，而不是其他类型，否则主控无法识别，导致无法出图，或者出图延时异常；比如vsync = SYNC_HIGH_LEVEL，表示sensor的有效信号是在vsync高时有效，而不是低有效，否则会导致主控采样异常，无法出图。其他成员变量类似，需要严格对应。

    步骤五：在dvp_xx.c中配置好对应的初始化表，分辨率子表，帧率子表等。

    步骤六：明确DVP硬件电源的控制GPIO，BK7258控制DVP的LDO也是通过GPIO28，拉高即可使能DVP的IOVDD，拉低即给DVP掉电，可以通过宏进行配置：

    +--------------------------------------+---------------+-------------------------------------+
    |                marco                 |     value     |           implication               |
    +--------------------------------------+---------------+-------------------------------------+
    | CONFIG_CAMERA_CTRL_POWER_GPIO_ID     |      0x1C     |         使用此GPIO控制电源          |
    +--------------------------------------+---------------+-------------------------------------+

    DVP还需要给DVDD供电，不同的摄像头，DVDD也是不一样的。

    步骤七：当前支持的功能配置较少，比如支持帧率的配置，如果需要额外支持曝光调节/图像翻转/图像黑夜模式等，需要增加上面结构体的成员，并给对应的成员赋对应的sensor配置表。

    步骤八：使用此工程，测试新适配的摄像头。


::

    //dvp摄像头结构体
    typedef struct
    {
        char *name;  /**< sensor name */
        media_ppi_t def_ppi;  /**< sensor default resolution */
        frame_fps_t def_fps;  /**< sensor default fps */
        mclk_freq_t  clk;  /**< sensor work clk in config fps and ppi */
        pixel_format_t fmt; /**< sensor output data format */
        sync_level_t vsync; /**< sensor vsync active level  */
        sync_level_t hsync; /**< sensor hsync active level  */
        uint16_t id;  /**< sensor type, sensor_id_t */
        uint16_t address;  /**< sensor write register address by i2c */
        uint16_t fps_cap;  /**< sensor support fps */
        uint16_t ppi_cap;  /**< sensor support resoultions */
        bool (*detect)(void);  /**< auto detect used dvp sensor */
        int (*init)(void);  /**< init dvp sensor */
        int (*set_ppi)(media_ppi_t ppi);  /**< set resolution of sensor */
        int (*set_fps)(frame_fps_t fps);  /**< set fps of sensor */
        int (*power_down)(void);  /**< power down or reset of sensor */
        int (*dump_register)(media_ppi_t ppi);  /**< dump sensor register */
        void (*read_register)(bool enable);  /**< read sensor register when write*/
    } dvp_sensor_config_t;

    一些参数的说明：
        * clk：摄像头输入的时钟，默认使用24MHz，需要根据摄像头规格书自行配置；
        * fmt：摄像头输出数据给芯片的格式，当前只支持YUV420，顺序需要根据摄像头输出顺序同步，默认YUYV；
        * vsync：摄像头输出vsync有效电平，有些摄像头vsync为低时，输出有效数据，需要与摄像头vsync输出电平同步，默认高电平有效；
        * hsync：摄像头输出vsync有效电平，有些摄像头vsync为低时，输出有效数据，需要与摄像头hsync输出电平同步，默认高电平有效；
        * address：配置摄像头寄存器的I2C slave地址，参需要根据摄像头规格书自行配置；
        * fps_cap：摄像头支持的帧率表，需要配置对应的寄存器来实现；
        * ppi_cap：摄像头支持的分辨率表，需要配置对应的寄存器来实现；
