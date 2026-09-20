/**
 * @file    boot_flow.c
 * @brief   BL 启动编排实现（SDK 的 services.bootloader + RTT 日志）
 * @version V1.0
 * @date    2026-09-18
 */

#include "boot_flow.h"

#include "ota_areas.h"
#include "bootloader.h"
#include "SEGGER_RTT_Log.h"

#define BOOT_BUF_LEN   4096u

static uint8_t    s_buf[BOOT_BUF_LEN];   /* MOVE 搬运缓冲（SWITCH 拓扑用不上但占着） */
static boot_ctx_t s_boot;                /* 勿放栈上：含 ota_cfg_t + 全部决策状态 */

static void rescue_loop(void) __attribute__((noreturn));

/**
 * @brief BL 里唯一能观察内部的窗口：全部翻译成 RTT 行。
 * @note  BOOT_EV_JUMP 之后 UART/外设都没了，但 RTT 缓冲在 RAM 里还在 ——
 *        JUMP 这行必须打，调试器接上去能看到最后一次决策。
 */
static void on_boot_event(boot_event_t ev, uint32_t arg, void *user)
{
    (void)user;
    switch (ev) {
    case BOOT_EV_CFG_FACTORY:     RTT_LOG("[BL] cfg: factory state"); break;
    case BOOT_EV_CFG_FAIL:        RTT_LOG("[BL] cfg save FAILED"); break;
    case BOOT_EV_ACTIVATE_SWITCH: RTT_LOG("[BL] activate SWITCH -> slot %u", arg); break;
    case BOOT_EV_ACTIVATE_MOVE:   RTT_LOG("[BL] activate MOVE %u bytes", arg); break;
    case BOOT_EV_MOVE_PROGRESS:   DBG_LOG("[BL] move/verify %u bytes", arg); break;
    case BOOT_EV_ACT_FAIL:        RTT_LOG("[BL] activate FAILED rc=%d", -(int)arg); break;
    case BOOT_EV_SLOT_BAD:        RTT_LOG("[BL] slot %u vector BAD", arg); break;
    case BOOT_EV_TRIAL:           RTT_LOG("[BL] trial try %u", arg); break;
    case BOOT_EV_ROLLBACK:        RTT_LOG("[BL] ROLLBACK -> slot %u", arg); break;
    case BOOT_EV_RESCUE:          RTT_LOG("[BL] RESCUE: no bootable image"); break;
    case BOOT_EV_JUMP:            RTT_LOG("[BL] jump -> slot %u (RTT buffer stays in RAM)", arg); break;
    default:                      break;
    }
}

static void rescue_loop(void)
{
    RTT_LOG("[BL] stopped: needs rescue (ST-Link / BOOT0 DFU)");
    for (;;) {
    }
}

void boot_flow_run(void)
{
    boot_cfg_t bc;
    int        rc;

    rc = ota_areas_setup();
    if (rc != OTA_OK) {
        ERR_LOG("[BL] ota_init FAILED rc=%d (check partition table / CFG)", rc);
        rescue_loop();
    }

    bc.buf     = s_buf;
    bc.buf_len = sizeof(s_buf);
    bc.max_try = 3u;
    bc.report  = on_boot_event;
    bc.user    = NULL;

    rc = boot_init(&bc);
    if (rc != OTA_OK) {
        ERR_LOG("[BL] boot_init FAILED rc=%d (partition table must have BOOT area)", rc);
        rescue_loop();
    }

    boot_start(&s_boot);
    for (;;) {
        boot_step_ret_t r = boot_step(&s_boot);

        if (r == BOOT_STEP_READY) {
            boot_jump(&s_boot);              /* 成功不返回 */
        } else if (r == BOOT_STEP_RESCUE || r == BOOT_STEP_ERR) {
            rescue_loop();
        }
        /* BUSY：继续下一步（MOVE 的长搬运被切成小步，这里可以加喂狗/点灯） */
    }
}
