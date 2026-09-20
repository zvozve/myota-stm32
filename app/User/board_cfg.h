#ifndef __BOARD_CFG_H
#define __BOARD_CFG_H

/*
 * board_cfg.h —— 工程侧硬件绑定（类 devicetree）
 *
 * 规则：
 *   - 全工程唯一允许 include CubeMX 生成头（main.h / usart.h / tim.h）的地方
 *     就是本文件；SDK（library/）不做任何绑定。
 *   - app / tasks 只引用本文件的绑定宏，不直接引用 MX 符号
 *     （CPU_STA_GPIO_Port / ETH_RST_Pin / &huart1 / &hiwdg ...）。
 *   - 换板只改本文件（引脚、句柄、时钟），SDK 与业务代码不动。
 */

#include "main.h"
#include "usart.h"
#include "iwdg.h"

/* ========== 功能开关（驱动读取，决定编译哪些传输/外设） ========== */
/* BOARD_USE_RTOS 同时是 SDK 的构建模式开关：chip 层（oop_dwt）据此切时基/延时，
 * devices/protocols/services/middleware/app 一律不得出现 RTOS 分支（差异由 chip 吸收）。 */
#define BOARD_USE_RTOS            1      /* 2.0 结构：FreeRTOS / CMSIS-RTOS2 宿主 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 心跳 LED + 看门狗 ========== */
#define BOARD_HEART_LED_PORT   CPU_STA_GPIO_Port
#define BOARD_HEART_LED_PIN    CPU_STA_Pin
#define BOARD_HEART_IWDG       (&hiwdg)        /* 无 IWDG 传 NULL */

/* ========== LAN8720A PHY 复位 ========== */
#define BOARD_ETH_RST_PORT     ETH_RST_GPIO_Port
#define BOARD_ETH_RST_PIN      ETH_RST_Pin

/* ========== OTA 下载口（YMODEM；USART6 = PC6/PC7 @115200，不与 Modbus 的 U1/U2 冲突） ========== */
#define BOARD_UART_OTA         (&huart6)

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CFG_H */
