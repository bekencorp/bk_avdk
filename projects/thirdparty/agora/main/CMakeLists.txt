set(incs . include)
set(srcs)

list(APPEND srcs
	app_main.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    agora_rtc_demo.c
    agora_sdk/agora_rtc.c
    audio/audio_transfer.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()


armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 agora-iot-sdk media_service multimedia)
