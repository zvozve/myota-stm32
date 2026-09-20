/**
 * @file    ota_areas.h
 * @brief   板级分区表（与 board_cfg.h 并列的工程侧资产，SDK 只认这张表）
 * @version V1.0
 * @date    2026-09-18
 */

#ifndef __OTA_AREAS_H
#define __OTA_AREAS_H

#include "ota_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  取内部介质 + 注册分区表 + ota_init()（含 CFG 区可对半分校验）
 * @return OTA_OK / OTA_ERR_*（分区表自检或 CFG 校验不过）
 * @note   BL 与 APP 都在各自 bring-up 的早期调它；失败就不要继续跑 OTA 流程。
 */
int ota_areas_setup(void);

#ifdef __cplusplus
}
#endif

#endif /* __OTA_AREAS_H */
