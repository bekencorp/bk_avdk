#pragma once

#include "aud_intf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AUD INTF API
 * @defgroup bk_aud_intf_api API group
 * @{
 */

/**
 * @brief     Init the AUD INTF driver
 *
 * This API init the resoure common:
 *   - Creat AUD INTF task
 *   - Configure default work mode
 *   - Register read mic data and write speaker data callback
 *
 * This API should be called before any other AUD INTF APIs.
 * This API cannot be called in mic and speaker callback.
 *
 * @param setup audio interface driver init configuration
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_drv_init(aud_intf_drv_setup_t *setup);

/**
 * @brief     Deinit the AUD INTF driver
 *
 * This API deinit the resoure common:
 *   - Delete AUD INTF task
 *   - Reset default work mode
 *   - Unregister read mic data and write speaker data callback
 *
 * This API should be called when audio is not needed.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_drv_deinit(void);

/**
 * @brief     Set the AUD INTF work mode
 *
 * This API cannot be called in mic and speaker callback.
 *
 * @param work_mode audio interface work mode
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_set_mode(aud_intf_work_mode_t work_mode);

/**
 * @brief     Set the mic gain
 *
 * @param value the gain value range:0x00 ~ 0x3f
 *
 * This API should be called when onboard mic has been initialized.
 * And this API should not be called when UAC mic has been used.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_set_mic_gain(uint8_t value);

/**
 * @brief     Set the spk gain(volume)
 *
 * @param value the gain value
 *    -board speaker: range:0x00 ~ 0x3f
 *    -uac speaker: range:0x0000 ~ 0xffff
 *
 * This API should be called when speaker has been initialized.
 * And this API set board speaker gain and uac speaker volume.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_set_spk_gain(uint32_t value);

/**
 * @brief     Set the aec parameter
 *
 * @param aec_para the parameter
 * @param value the parameter value
 *
 * This API should be called when voice has been initialized and AEC is enable.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_set_aec_para(aud_intf_voc_aec_para_t aec_para, uint32_t value);

/**
 * @brief     Get the aec parameter
 *
 * This API should be called when voice has been initialized and AEC is enable.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_get_aec_para(void);

/**
 * @brief     Register callback of uac abnormal disconnection and connection recovery
 *
 * This API should be called after bk_aud_intf_voc_init.
 * This API only support uac mic and uac speaker.
 * This API cannot be called in mic and speaker callback.
 *
 * @param cb callback api
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_register_uac_connect_state_cb(void *cb);

/**
 * @brief     Control uac automatical connect
 *
 * If enable the feature, uac will automatic connect after uac abnormal disconnect.
 * This API only support uac mic and uac speaker.
 * This feature is enabled by default.
 * This API cannot be called in mic and speaker callback.
 *
 * @param enable
 *    - TRUE: enable uac automatic connnet
 *    - FALSE: disable uac automatic connent
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_uac_auto_connect_ctrl(bool enable);


/*************************************** voice api *********************************************/
/**
 * @brief     Init the AUD INTF voice
 *
 * This API init the resoure common:
 *   - Init AUD voice transfer driver
 *   - Configure AUD dac and adc paramter
 *   - Configure DMA to carry adc and dac data
 *   - Configure UAC mic and speaker
 *
 * This API should be called after bk_aud_intf_drv_init.
 * This API cannot be called in mic and speaker callback.
 *
 * @param voc_setup audio voice setup configuration
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 *
 * Usage example:
 *
 *     audio_tras_setup_t aud_tras_setup = DEFAULT_AUD_INTF_DRV_SETUP_CONFIG();
 *     aud_intf_voc_setup_t aud_voc_setup = DEFAULT_AUD_INTF_VOC_SETUP_CONFIG();
 *
 *     aud_intf_drv_setup.aud_intf_tx_mic_data = demo_udp_voice_send_packet;
 *     bk_aud_intf_drv_init(&aud_intf_drv_setup);
 *
 *     aud_work_mode = AUD_INTF_WORK_MODE_VOICE;
 *     bk_aud_intf_set_mode(aud_work_mode);
 *
 *     bk_aud_intf_voc_init(aud_voc_setup);
 *
 */
bk_err_t bk_aud_intf_voc_init(aud_intf_voc_setup_t setup);

/**
 * @brief     Deinit the AUD INTF voice
 *
 * This API deinit the resoure common:
 *   - Deinit AUD voice transfer driver
 *   - Reset AUD adc and dac paramter
 *   - Close DMA
 *
 * This API should be called after bk_aud_intf_voice_stop.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_deinit(void);

/**
 * @brief     Start the AUD INTF voice work
 *
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_start(void);

/**
 * @brief     Stop the AUD INTF voice work
 *
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_stop(void);

/**
 * @brief     Control mic
 *
 * @param mic_en
 *    - AUD_INTF_VOC_MIC_OPEN: enable mic
 *    - AUD_INTF_VOC_MIC_CLOSE: disable mic
 *
 * This API should be called in voice work mode.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_mic_ctrl(aud_intf_voc_mic_ctrl_t mic_en);

/**
 * @brief     Control speaker
 *
 * @param spk_en
 *    - AUD_INTF_VOC_SPK_OPEN: enable speaker
 *    - AUD_INTF_VOC_SPK_CLOSE: disable speaker
 *
 * This API should be called in voice work mode.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_spk_ctrl(aud_intf_voc_spk_ctrl_t spk_en);

/**
 * @brief     Control aec enable or disable
 *
 * @param aec_en
 *    - true: enable aec
 *    - false: disable aec
 *
 * This API should be called in voice work mode.
 * This API cannot be called in mic and speaker callback.
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_voc_aec_ctrl(bool aec_en);

/**
 * @brief     Write the speaker data to audio dac
 *
 * This API should be called in general and voice work mode.
 * This API cannot be called in mic and speaker callback.
 *
 * @param dac_buff the address of speaker data written
 * @param size the data size (byte)
 *
 * @return
 *    - BK_OK: succeed
 *    - others: other errors.
 */
bk_err_t bk_aud_intf_write_spk_data(uint8_t *dac_buff, uint32_t size);

/**
 * @}
 */


#ifdef __cplusplus
}
#endif

