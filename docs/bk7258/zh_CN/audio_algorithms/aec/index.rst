AEC 调试
=================================

:link_to_translation:`en:[English]`

1、功能概述
--------------------

    AEC回声消除算法主要用于双向语音通话场景，通过算法消除设备端mic采集到的speaker播放的音频得到干净的人声。

2、工程代码路径
----------------------------------------

	AEC算法的使用参考demo路径如下：``./components/audio_algorithm/aec``

	demo中提供了测试验证AEC算法pcm数据和接口的使用方法。

3、aec回声消除效果调试
-------------------------------

	AEC调试注意事项：

.. important::
	回声的情况和设备的mic灵敏度、mic增益、speaker灵敏度、speaker增益、产品外壳布局、音强等息息相关。不同的回声情况需要不同的AEC参数来处理，因此在开始调试AEC参数前必须确保完成以下前提工作：

	- 1. mic型号必须固定，不同型号其灵敏度也会有所不同，灵敏度越高越回声越大，因此在满足使用场景的前提下可以尽量选用灵敏度低的mic。
	- 2. mic的增益必须固定，增益的大小会影响音频信号的幅值。
	- 3. speaker型号必须固定，不同型号其功率的大小也会有所不同，功率越大声音越大，回声也会越大，因此在满足使用场景的前提下可以尽量选用功率低的speaker。
	- 4. speaker的增益（包含PA和数字芯片内部的DAC增益）必须固定，增益越大，speaker的音量越大，回声也会越大，因此在满足使用场景的前提下可以尽量调低speaker的增益。
	- 5. 产品外壳和音腔必须固定，不同的外壳和音腔会导致设备的回声情况不同，因此必须固定产品外壳和音腔。

基于aud_intf组件开发的语音通话应用需要调试AEC回声消除的效果，主要需要调试以下几个参数 ``echo depth`` 、 ``max amplitude`` 、 ``min amplitude`` 、 ``noise level`` 和 ``noise param`` 。

调试步骤如下：
	- 1、确保通话的两个设备（通常是手机和板子）处于远程状态，确保手机端说话的声音不会被板子端的mic采集到，然后打开语音通话;
	- 2、根据听到的回声效果, 使用通过uart发送串口指令 ``aud_intf_set_aec_param_test {param value}`` 设置各参数的值，在线调试AEC参数;
	- 3、首先发送串口指令 ``aud_intf_set_aec_param_test ec_depth {value}`` 来设置 ``echo depth`` , 回声越大值设置越大, 当继续增大该值, 但是无法提升回声消除效果时停止设置该值, 建议取值范围 ``1~50`` ;
	- 4、其次根据回声的大小范围分别发送串口指令 ``aud_intf_set_aec_param_test TxRxThr {value}`` 和 ``aud_intf_set_aec_param_test TxRxFlr {value}`` 来设置 ``max amplitude`` 和 ``min amplitude`` 的值, 优化回声消除效果;
	- 5、然后根据听到的底噪大小发送串口指令 ``aud_intf_set_aec_param_test ns_level {value}`` 来设置 ``noise level`` 底噪越大，设置的值越大, 优化底噪消除效果, 建议取值范围 ``1~8`` ;
	- 4、最后发送串口指令 ``aud_intf_set_aec_param_test ns_para {value}`` 来设置 ``noise param`` 的值, 分别设置为0、1、2，选择效果最好的值;
	- 5、发送串口指令 ``aud_intf_get_aec_param_test`` 获取并记录下所有参数的在线调试结果, 并设置为语音初始化时AEC的默认参数。

.. note::
 - 1. 打开宏 ``CONFIG_AUD_INTF_TEST=y`` 才能确保串口指令 ``aud_intf_set_aec_param_test {param value}`` 生效。
 - 2. AEC参数没有调优会导致板子对端（通常是手机端）听到的声音偶现异常，例如：偶现无声、偶现有部分回声等。

4、参考链接
----------------------------------------

    `API 参考 : <../../api-reference/multi_media/bk_aec.html>`_ 介绍AEC API接口

    `用户开发指南 : <../../audio_algorithms/aec/index.html>`_ 介绍AEC调试开发流程
