set(incs . include)
set(srcs main.c)



if ("${ARMINO_SOC}" STREQUAL "bk7258")
list(APPEND priv_req multimedia )
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}" 
	PRIV_REQUIRES multimedia bk_init bk_cli media_service bt 
)
