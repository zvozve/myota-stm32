/**
 * @file    boot_flow.h
 * @brief   BL 的启动编排（工程侧最后一层：SDK 的 bootloader 只给状态机）
 * @version V1.0
 * @date    2026-09-18
 */

#ifndef __BOOT_FLOW_H
#define __BOOT_FLOW_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  BL 主流程：注册分区表 → boot_init（挂安全闸）→ 步进决策 → 跳转
 * @note   **不返回**。任何一步走不通（分区表坏 / 无可用镜像 / 步进出错）都会
 *         停在救援循环里，RTT 里有原因；此时设备只能靠 ST-Link / BOOT0 兜底。
 */
void boot_flow_run(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_FLOW_H */
