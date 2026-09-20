/**
 * @file    app_ota.h
 * @brief   APP 侧 OTA 接线：早期 VTOR / 下载口（USART6）/ 触发与轮询
 * @version V1.0
 * @date    2026-09-18
 *
 * 接线点（共三处）：
 *   ① main.c 的 USER CODE 1 —— `app_ota_early_init()`，必须是 main 的第一条语句
 *      （VTOR 指向本槽，从 BL 跳过来时才不会把中断映到另一个槽）
 *   ② App_Init() —— `app_ota_setup()`（注册分区表 + 打开下载口）
 *   ③ vAppTask 循环 —— `app_ota_poll()`（触发下载 + 非阻塞推进）
 *
 * 触发方式：下载口（USART6，PC6=TX / PC7=RX @115200）收到字符 'U' 即开始。
 * PC 端用 SDK 的 `tools/fw-ota-ymodem.py`（VSC 任务「OTA YMODEM」，弹窗选 .bin；
 * 默认「元数据优先」= 方案 2）：
 *   ① 先发 8 字节元数据 = size(u32 LE) + crc32(u32 LE)（crc32 与设备 ota_crc32 一致）
 *   ② 再发固件本体 raw .bin
 * 设备先收元数据（得到期望大小 / 期望 CRC32），再收 bin，按期望 CRC32 校验整段镜像
 * （而非只做「内存 CRC == 闪存 CRC」自比，可发现 PC 端 bin 本身损坏）。
 */

#ifndef __APP_OTA_H
#define __APP_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief main 第一行：设 VTOR + 开中断。**必须先于 HAL_Init()** */
void app_ota_early_init(void);

/** @brief 注册分区表 + 初始化下载口。放 App_Init()，失败不致命但 OTA 不可用 */
int app_ota_setup(void);

/** @brief 任务循环里调（非阻塞）：'U' 触发下载；下载中推进；完成时软复位交给 BL */
void app_ota_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_OTA_H */
