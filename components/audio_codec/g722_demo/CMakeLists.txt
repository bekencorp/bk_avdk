if (CONFIG_AUDIO_G722_TEST)
set(srcs
	g722_demo.c
	)
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_common)