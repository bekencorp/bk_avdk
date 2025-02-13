set(incs include)
set(srcs "")
set(priv_req bk_common avdk_utils)

if (CONFIG_DVP_CAMERA)
	list(APPEND srcs
		src/bk_dvp.c
		src/video_common_driver.c
	)
endif()


#list(APPEND priv_req unity include)

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")
