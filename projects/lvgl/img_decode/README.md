本工程用于图像JPEG硬件解码、JPEG软件解码以及PNG软件解码的演示，主要通过宏CONFIG_LV_IMG_UTILITY_CUSTOMIZE进行控制使能。
demo工程中当前主要演示了JPEG图像软件解码，图像资源已通过fatfs工具打包成bin文件，文件路径见SDK的project/lvgl/img_decode/resource/fatfs.bin文件。
具体测试方法:
1、使用1024x600分辨率的HX8282的LCD；
2、将fatfs.bin文件烧录到flash的usr_config分区，即0x4ea000地址；
3、输入编译命令：make bk7258 PROJECT=lvgl/img_decode
4、烧录all-app.bin后上电执行即可。