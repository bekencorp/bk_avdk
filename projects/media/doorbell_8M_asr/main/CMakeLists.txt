set(incs . include)
set(srcs app_main.c)

if (CONFIG_INTEGRATION_DOORBELL)
if (CONFIG_SYS_CPU0)
list(APPEND srcs
	src/doorbell_core.c
	src/doorbell_boarding.c
	src/doorbell_sdp.c
	src/doorbell_network.c
	src/doorbell_transmission.c
	src/doorbell_udp_service.c
	src/doorbell_tcp_service.c
	src/doorbell_devices.c
	src/doorbell_cmd.c
)
endif()
endif()

if (CONFIG_INTEGRATION_DOORBELL_CS2)
list(APPEND srcs
	src/doorbell_cs2_service.c
	)
endif()

if (CONFIG_AUD_ASR)
list(APPEND srcs asr/aud_asr.c)
if (CONFIG_SYS_CPU2)
if (CONFIG_WENWEN_ASR)
list(APPEND srcs asr/aud_asr_cp2_wenwen.c)
endif()
if (CONFIG_WANSON_ASR)
list(APPEND srcs asr/aud_asr_cp2_wanson.c)
endif()
endif()
endif()

set(priv_req bk_init bt media_service lwip_intf_v2_1 wifi_transfer avdk_utils multimedia display_service wanson wenwen)

if ("${ARMINO_SOC}" STREQUAL "bk7258")
list(APPEND priv_req cs2p2p )
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
	list(APPEND srcs
		vendor_flash.c
	)
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES "${priv_req}"
)
