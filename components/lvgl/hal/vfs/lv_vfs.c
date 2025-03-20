#include "os/os.h"
#include "bk_posix.h"
#include "driver/flash_partition.h"
#if CONFIG_LITTLEFS_USE_LITTLEFS_PARTITION
#include "vendor_flash_partition.h"
#endif


#if (CONFIG_FATFS)
static int _fs_mount(void)
{
    struct bk_fatfs_partition partition;
    char *fs_name = NULL;
    int ret;

    fs_name = "fatfs";
    partition.part_type = FATFS_DEVICE;
#if (CONFIG_SDCARD)
    partition.part_dev.device_name = FATFS_DEV_SDCARD;
#else
    partition.part_dev.device_name = FATFS_DEV_FLASH;
#endif
    partition.mount_path = "/";

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#endif

#if (CONFIG_LITTLEFS)
static int _fs_mount(void)
{
    int ret;

    struct bk_little_fs_partition partition;
    char *fs_name = NULL;
#ifdef BK_PARTITION_LITTLEFS_USER
    bk_logic_partition_t *pt = bk_flash_partition_get_info(BK_PARTITION_LITTLEFS_USER);
#else
    bk_logic_partition_t *pt = bk_flash_partition_get_info(BK_PARTITION_USR_CONFIG);
#endif

    fs_name = "littlefs";
    partition.part_type = LFS_FLASH;
    partition.part_flash.start_addr = pt->partition_start_addr;
    partition.part_flash.size = pt->partition_length;
    partition.mount_path = "/";

    ret = mount("SOURCE_NONE", partition.mount_path, fs_name, 0, &partition);

    return ret;
}
#endif

bk_err_t lv_vfs_init(void)
{
    bk_err_t ret = BK_FAIL;

    do {
        ret = _fs_mount();
        if (BK_OK != ret)
        {
            bk_printf("[%s][%d] mount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
            break;
        }

        bk_printf("[%s][%d] mount success\r\n", __FUNCTION__, __LINE__);
    } while(0);

    return ret;
}

bk_err_t lv_vfs_deinit(void)
{
    bk_err_t ret = BK_FAIL;

    ret = umount("/");
    if (BK_OK != ret) {
        bk_printf("[%s][%d] unmount fail:%d\r\n", __FUNCTION__, __LINE__, ret);
    }

    bk_printf("[%s][%d] unmount success\r\n", __FUNCTION__, __LINE__);

    return ret;
}

