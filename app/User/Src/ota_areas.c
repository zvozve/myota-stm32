/**
 * @file    ota_areas.c
 * @brief   板级分区表 —— 1 MB 内部 A/B（doc/02 定稿）
 * @version V1.0
 * @date    2026-09-18
 *
 *   BL 32K @0x08000000 | CFG 32K @0x08008000 | SlotA 448K @0x08010000 | SlotB 512K @0x08080000
 *
 * 对齐核对（F407ZG 扇区：S0-S3 各 16K，S4 64K，S5-S11 各 128K）：
 *   - boot  32K = S0+S1，起点对齐 S0
 *   - cfg   32K = S2+S3；半区 16K 恰好一个扇区（ota_cfg_init 的硬性要求）
 *   - slotA 448K = S4(64K)+S5+S6+S7(384K)，起点 0x08010000 对齐 S4
 *   - slotB 512K = S8+S9+S10+S11，起点 0x08080000 对齐 S8
 * 改任何一个数都要同时核对 .ld、CMake 的 OTA_SELF_BASE、以及上面的扇区对齐。
 */

#include "ota_areas.h"

static ota_flash_t s_flash_int;   /* 内部介质实例（ota_areas_setup 填充，须常驻） */

/** @note base 是「介质内偏移」：内部 Flash 的偏移 0 == 0x08000000 */
static const ota_area_t s_areas[] = {
    { "boot",  OTA_AREA_ROLE_BOOT, OTA_SLOT_NONE, 0u, 0x000000u,   32u * 1024u },
    { "cfg",   OTA_AREA_ROLE_CFG,  OTA_SLOT_NONE, 0u, 0x008000u,   32u * 1024u },
    { "slotA", OTA_AREA_ROLE_RUN,  OTA_SLOT_A,    0u, 0x010000u,  448u * 1024u },
    { "slotB", OTA_AREA_ROLE_RUN,  OTA_SLOT_B,    0u, 0x080000u,  512u * 1024u },
};

int ota_areas_setup(void)
{
    ota_env_t env;
    int       rc;

    rc = ota_flash_int_get(&s_flash_int);
    if (rc != OTA_OK) {
        return rc;
    }

    env.areas       = s_areas;
    env.area_count  = (uint8_t)(sizeof(s_areas) / sizeof(s_areas[0]));
    env.flashes     = &s_flash_int;
    env.flash_count = 1u;

    return ota_init(&env);
}
