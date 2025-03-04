#include <common/bk_include.h>
#include <components/log.h>

#if CONFIG_MEDIA
#include "media_core.h"
#endif

#include "media_ipc.h"
#include "bk_peripheral.h"

#include "audio_osi_wrapper.h"
#include "video_osi_wrapper.h"

#define TAG "ME INIT"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)



int media_service_init(void)
{
	bk_err_t ret = BK_OK;

#if (CONFIG_MEDIA)
#if (CONFIG_SOC_BK7258 && CONFIG_SYS_CPU1)
	bk_peripheral_init();
#endif
#endif

	ret = bk_video_osi_funcs_init();

	if (ret != kNoErr)
	{
		LOGE("%s, bk_video_osi_funcs_init failed\n", __func__);
		return ret;
	}
	ret = bk_audio_osi_funcs_init();

	if (ret != kNoErr)
	{
		LOGE("%s, bk_audio_osi_funcs_init failed\n", __func__);
		return ret;
	}

#ifdef CONFIG_MEDIA_IPC
	media_ipc_init();
#endif

#if (CONFIG_MEDIA && CONFIG_SOC_BK7258)
#if (CONFIG_MEDIA_MAJOR)
	media_major_mailbox_init();
#elif (CONFIG_MEDIA_MINOR)
	media_minor_mailbox_init();
#else
	media_app_mailbox_init();
	extern int media_cli_init(void);
	media_cli_init();
#endif
#endif

#if (CONFIG_MEDIA_MAJOR | CONFIG_SYS_CPU1)
#else

#if (CONFIG_AUD_INTF_TEST)
	extern int cli_aud_intf_init(void);
	cli_aud_intf_init();
#endif

#endif

#if (CONFIG_USB_CDC)
	extern bk_err_t bk_cdc_acm_demo(void);
	bk_cdc_acm_demo();
#endif
	return 0;
}
