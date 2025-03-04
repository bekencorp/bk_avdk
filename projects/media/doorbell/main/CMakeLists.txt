set(incs . include)
set(srcs app_main.c)

if (CONFIG_INTEGRATION_DOORBELL)
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

if (CONFIG_INTEGRATION_DOORBELL_CS2)
list(APPEND srcs
	src/doorbell_cs2_service.c
)
endif()

if (CONFIG_BLEND_UI)
list(APPEND incs
	src/assets
)

list(APPEND srcs
	src/assets/bk_font.c
	src/assets/bk_img.c
	src/assets/blend_dsc.c
)
endif()

set(priv_req bk_init bt media_service lwip_intf_v2_1 wifi_transfer avdk_utils multimedia bk_draw_blend)

if ("${ARMINO_SOC}" STREQUAL "bk7258")
list(APPEND priv_req cs2p2p)
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
