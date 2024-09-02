DVP
===============================

:link_to_translation:`zh_CN:[中文]`

1. Introduction
---------------------------------

This project is the default debugging project for DVP, which can be used to test whether the inserted DVP works properly. The default is 16M PSRAM.

1.1 Specifications
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Please refer to `Specifications <../../media/doorviewer/index.html#specifications>`_

1.2 Path
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    <bk_avdk source code path>/projects/peripheral/dvp

2. Framework diagram
---------------------------------

    Please refer to `Framework diagram <../../media/doorviewer/index.html#framework-diagram>`_

3. Configuration
---------------------------------

3.1 Bluetooth and Multimedia Memory Reuse
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    Using this project is only for verifying DVP, so the memory does not need to be shared with Bluetooth.

    +-------------------------------------+---------------+---------------------------------------------------------------------+
    |          marco                      |     value     |                       implication                                   |
    +-------------------------------------+---------------+---------------------------------------------------------------------+
    | CONFIG_BT_REUSE_MEDIA_MEMORY        |       N       | Multimedia and Bluetooth share one SRAM (time-division multiplexing)|
    +-------------------------------------+---------------+---------------------------------------------------------------------+

3.1.1 Uninstalling Bluetooth
.................................

::

    #ifdef CONFIG_BT_REUSE_MEDIA_MEMORY
    #if CONFIG_BLUETOOTH
        bk_bluetooth_deinit();
    #endif
    #endif

3.1.2 Initialize Bluetooth

.................................

::

    bk_bluetooth_init();

3.2 Differences
,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    The difference between DVP and Doorviewer is that:
        * DVP starts by default when powered on, and analyzes the output JPEG image raw data format YUV422/YUV420 and output resolution through a software decoder. Other peripherals are not involved and do not support image transfer, LCD display, voice transmission, etc.
        * Doorviewer is a complete door lock solution.

4. Demonstration explanation
---------------------------------

    Default power on self start, no additional operation required, Print the following log.

::

    (7598):##MJPEG:camera_type:1(1:dvp 2:uvc) frame_id:178, length:26163, frame_addr:0x6011f7a0
    video_co:I(7598):4:2:2, YUV:
    (7598):##DECODE:pixel_x:640, pixel_y:480
    (7600):rotate_angle:0(0:0 1:90 2:180 3:270)
    (7600):byte_order:0(0:little endian 1:big endian)
    (7600):out_fmt:YUYV

5. Code explanation
---------------------------------

    Please refer to `Code explanation <../../media/doorviewer/index.html#code-explanation>`_

6. Add adaptation
---------------------------------

    When adapting a new camera, the debugging steps are as follows:

    Step 1: Refer to the configuration methods of other peripherals: ``./components/bk_peripheral/src/dvp/dvp_gc2145.c``,
    Add a new dvp_xx.c file to the corresponding folder. And add the newly added peripherals to ``dvp_sensor_devices.c``,
    modify the directory's: "./components/bk_peripheral" CMakeList.txt file, add the new file dvp_xx.c to compiler.

    Step 2: Clearly specify the I2C read/write address and the hardware I2C ID used in dvp_xx.c. BK7258 defaults to I2C1, corresponding to GPIO0 and GPIO1.
    The I2C ID used by DVP is configured through macros.

    +------------------------------+---------------+-------------------------------------+
    |             marco            |     value     |           implication               |
    +------------------------------+---------------+-------------------------------------+
    | CONFIG_DVP_CAMERA_I2C_ID     |       1       |        The index of using I2C       |
    +------------------------------+---------------+-------------------------------------+

    Step 3: Clarify the CHIP_ID of the camera in dvp_xx.c.

    Step 4: Clarify the members of the structure "dvp_sensor_config. t" in dvp_xx.c. Note that the configured parameters need to be consistent with the camera configuration.
    For example, CLK=MCLK_24 indicates that the input clock of the sensor is 24MHz,

    Instead of other values, otherwise it will be inconsistent with the expected frame rate; For example,
    fmt=PIXEL_FMT_YUYV indicates that the format of the sensor's output image is YUYV, not other types. Otherwise,
    the main control cannot recognize it, resulting in the inability to output the image or abnormal output delay; For example, vsync=SYNC=HIGHVNet,
    which means that the effective signal of the sensor is effective when vsync is high, not low,
    otherwise it will cause abnormal sampling of the main control and result in the inability to generate a graph.
    Other member variables are similar and require strict correspondence.

    Step 5: Configure the corresponding initialization table, resolution sub table, frame rate sub table, etc. in dvp_xx.c.

    Step 6: Clarify the control GPIO of the DVP hardware power supply. BK7258 also controls the LDO of the DVP through GPIO28.
    Pulling it high will enable the IOVDD of the DVP, and pulling it low will power down the DVP. This can be configured through macros:

    +--------------------------------------+---------------+-------------------------------------+
    |                marco                 |     value     |           implication               |
    +--------------------------------------+---------------+-------------------------------------+
    | CONFIG_CAMERA_CTRL_POWER_GPIO_ID     |      0x1C     |   The GPIO ID control dvp power     |
    +--------------------------------------+---------------+-------------------------------------+

    DVP also needs to supply power to the DVDD, which varies depending on the camera.

    Step 7: Currently, there are few supported feature configurations, such as frame rate configuration.
    If additional support is needed for exposure adjustment, image flipping, image night mode, etc.
    It is necessary to add members to the above structure and assign corresponding sensor configuration tables to the corresponding members.

    Step 8: Use this project to test the newly adapted camera.


::

    //dvp camera struct
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

    Description of some parameters:
        * clk: clock input for the camera. The default clock is 24MHz, which needs to be configured according to the camera specification.
        * fmt: The format of camera output data to the chip. Currently, only YUV420 is supported, and the sequence needs to be synchronized according to the camera output sequence. The default is YUYV.
        * vsync: camera output vsync effective level. When some cameras vsync is low, the output valid data needs to be synchronized with the camera vsync output level. The default high level is valid.
        * hsync: effective level of camera output vsync. When vsync of some cameras is low, the output of valid data needs to be synchronized with the hsync output level of the camera. The default high level is valid.
        * address: Configure the I2C slave address of the camera register. The parameter needs to be configured according to the camera specification.
        * fps_cap: The frame rate table supported by the camera, which needs to be configured with the corresponding register.
        * ppi_cap: resolution table supported by camera, need to configure the corresponding register to achieve.