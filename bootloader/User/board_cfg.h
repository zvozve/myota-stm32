#ifndef __BOARD_CFG_H
#define __BOARD_CFG_H

/*
 * board_cfg.h —— BL 工程的板级绑定（最小骨架）
 *
 * BL 按设计**不联网、不点灯、不开 UART**（32K 里塞不下，救援方案见 todo.md），
 * 所以没有任何 MX 句柄 / 引脚要注入。SDK 侧 boot_init() 会用 oop_flash 的
 * 安全闸把 [BOOT 区末尾, Flash 末尾) 设成唯一可擦写窗口，分区表见
 * User/Src/ota_areas.c（板级事实的另一半在这里）。
 */

#include "main.h"

/* ========== 功能开关（SDK 读取；与绑定区解耦） ========== */

/* BOARD_USE_RTOS：宿主是 RTOS(1) 还是裸机(0)。SDK 据此归一化为 MB_USE_RTOS，且只在
 * chip/ 层使用（时基/延时自动切换）；devices/protocols/services/middleware/app 一律
 * 不得出现 RTOS 分支。改这一行即可切换，不必再动 CMakeLists。 */
#define BOARD_USE_RTOS            0      /* BL 无 RTOS：时基走 HAL(SysTick)，延时走 DWT 忙等 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CFG_H */
