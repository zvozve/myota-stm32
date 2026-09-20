/**
 * @file    app_ota.c
 * @brief   APP 侧 OTA 接线实现
 * @version V1.0
 * @date    2026-09-18
 */

#include "app_ota.h"

#include "ota_areas.h"
#include "ota_app.h"
#include "ota_src_uart.h"
#include "ota_area.h"           /* ota_area_select_target()：算目标槽回报给主机 */

#include "board_cfg.h"          /* 唯一允许引用 MX 符号处（&huart6 经 BOARD_UART_OTA） */
#include "oop_uart_drv.h"
#include "heart_beat.h"
#include "SEGGER_RTT_Log.h"

/* 开关唯一来源 = User/board_cfg.h 的 BOARD_USE_RTOS（与 app_main.c 同一个真相源）。
 * 这里不能自己再 #define APP_USE_RTOS —— 那样本文件的判断就与本工程模式脱钩。 */
#if APP_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#ifndef OTA_SELF_BASE
#error "OTA_SELF_BASE 未定义：CMake 按槽传入（A=0x08010000 / B=0x08080000）"
#endif

/* 搬运缓冲 = 单次 flash 写入量。取 1024（= 一个 YMODEM 包）而不是 4096：
 *   ① 写入耗时更短（F4 256 字 ≈ 数毫秒），流内长阻塞更少见；
 *   ② UART 驱动只有一个 1088 B 的帧缓冲，单次阻塞越短，越不可能丢字节；
 *   ③ 本板 RAM 只剩 2 KB 出头，4096 太大。 */
#define APP_OTA_BUF_LEN    1024u
#define APP_OTA_TRIGGER    'U'

static ota_app_t      s_ota;
static ota_src_uart_t s_src;
static uart_drv_t     s_uart;
static uint8_t        s_buf[APP_OTA_BUF_LEN];

/* ---------------- 回调（先定义，接线函数在后面引用） ---------------- */

static int s_ota_target_printed = 0;

static void on_flow_report(ota_flow_t *f, void *user)
{
    (void)user;
    if (f->state == OTA_FLOW_HDR) {
        s_ota_target_printed = 0;          /* 新一轮下载，重置「已打印目标槽」标记 */
    }
    if (f->state == OTA_FLOW_ERASE && s_ota_target_printed == 0) {
        s_ota_target_printed = 1;
        /* do_decide 成功后才进 ERASE：此时目标槽/大小/期望 CRC 都已确定。
         * （在 DECIDE 上报会打印到「尚未决定」的旧值 255/0。） */
        RTT_LOG("[OTA] RX -> slot %u  %u bytes  (%s%s)  expect_crc32=0x%08X",
                (unsigned)f->target_slot, (unsigned)f->total,
                f->raw ? "raw-bin" : "otapkg",
                (f->raw && f->seg.crc32 != 0u) ? "+meta" : "",
                (unsigned)f->seg.crc32);
    }
    RTT_LOG("[OTA] %-13s %3u%% err=%d",
            ota_flow_state_name(f->state), ota_flow_progress(f), (int)f->err);
}

/**
 * @brief 等包期间的钩子：喂狗 + 让出 CPU
 * @note  单次 read 最长阻塞 poll_timeout_ms（默认 2000ms），而本板 IWDG 只有
 *        512ms —— 不在这里喂狗，下载必然被看门狗复位。vTaskDelay 顺带避免满速自旋。
 */
static int ota_idle(void *user)
{
    (void)user;
    heart_beat_run();
#if APP_USE_RTOS
    vTaskDelay(pdMS_TO_TICKS(1));
#endif
    return 0;
}

/* ---------------- 三处接线 ---------------- */

void app_ota_early_init(void)
{
    /* 只做设 VTOR + 开中断两件事。此时 HAL 还没 Init，别加任何外设操作。 */
    (void)ota_app_early_init(&s_ota, (uint32_t)OTA_SELF_BASE);
}

int app_ota_setup(void)
{
    int rc = ota_areas_setup();

    if (rc != OTA_OK) {
        ERR_LOG("[OTA] ota_init FAILED rc=%d (check partition table / CFG align)", rc);
        return rc;
    }

    uart_drv_init(&s_uart, BOARD_UART_OTA, NULL);
    ota_src_uart_init(&s_src, &s_uart, NULL, NULL);
    ota_src_uart_set_idle_cb(&s_src, ota_idle, NULL);

    ota_app_set_report_cb(&s_ota, on_flow_report, NULL);

    RTT_LOG("[OTA] ready: self=0x%08X slot=%u, trigger '%c' on UART6",
            (unsigned)OTA_SELF_BASE, (unsigned)s_ota.self_slot, APP_OTA_TRIGGER);
    return OTA_OK;
}

/* 方案 2（元数据优先）：'U' 之后先收 8 字节元数据(size+crc32)，再收 bin 本体 */
static uint8_t s_meta_phase = 0u;

/* 'U' 之后先回一行「我要收哪个槽」，主机据此自动挑 A/B 镜像（不必人工选分区）：
 *      #OTA target=A\n        （算不出目标槽时 target=-）
 * 发送必须排在 release_packet 之后 —— 驱动被 get_packet 锁住时 send 返回 -3 拒发；
 * 且要等这一行真正发完才起元数据段（否则元数据的握手 'C' 撞 TX_BUSY 被拒发）。
 * 投递不出去也不许卡死 OTA：计满次数就跳过，直接进元数据段。 */
#define APP_OTA_ANNOUNCE_MAX_TRY   50u

static uint8_t s_announce     = 0u;    /* 0=无 / 1=待投递 / 2=已投递，等发完 */
static uint8_t s_announce_try = 0u;

static void ota_announce_step(void)
{
    if (s_announce == 1u) {
        static const char pre[] = "#OTA target=";
        ota_target_t      tgt;
        uint8_t           msg[20];
        uint8_t           n    = 0u;
        uint8_t           slot = OTA_SLOT_NONE;

        /* 目标槽由 SDK 算：它会自动避开当前运行槽（A/B 拓扑即「另一个槽」） */
        if (ota_area_select_target(ota_app_self_slot(&s_ota), &tgt) == OTA_OK) {
            slot = tgt.target_slot;
        }
        while (pre[n] != '\0') {
            msg[n] = (uint8_t)pre[n];
            n++;
        }
        msg[n++] = (slot < OTA_SLOT_COUNT) ? (uint8_t)('A' + slot) : (uint8_t)'-';
        msg[n++] = (uint8_t)'\n';

        if (uart_drv_send(&s_uart, msg, n) == 0) {
            s_announce = 2u;
        } else if (++s_announce_try >= APP_OTA_ANNOUNCE_MAX_TRY) {
            ERR_LOG("[OTA] announce rejected -> skip, metadata next");
            s_announce = 2u;
        }
        return;
    }

    if (s_announce == 2u) {
        if (uart_drv_get_state(&s_uart) == UART_DRV_TX_BUSY) {
            return;                                     /* 还没发完，别抢 TX */
        }
        s_announce     = 0u;
        s_announce_try = 0u;
        if (ota_src_uart_meta_start(&s_src) == OTA_OK) {
            s_meta_phase = 1u;
        } else {
            ERR_LOG("[OTA] meta_start FAILED");
        }
    }
}

void app_ota_poll(void)
{
    if (s_ota.busy != 0u) {
        int r = ota_app_process(&s_ota);

        if (r == OTA_APP_DONE) {
            RTT_LOG("[OTA] committed -> reboot into BL");
            ota_app_reboot();                       /* 不返回 */
        } else if (r == OTA_APP_ERR) {
            /* FLASH_SR 打印是临时诊断：区分「FLASH 错误标志残留」与「地址越界」
             * 这两类擦写失败（BL 写 CFG 遗留 OPERR/WRPERR/PGAERR/... 会让本轮
             * 首次擦除瞬时失败）。定位后可连同本行一起删。 */
            ERR_LOG("[OTA] download FAILED rc=%d FLASH_SR=0x%08X",
                    (int)s_ota.flow.err, (unsigned)FLASH->SR);
        }
        return;
    }

    /* 阶段 0：'U' 之后先回报目标槽（主机据此自动挑 A/B 镜像），等 TX 发完再进元数据段 */
    if (s_announce != 0u) {
        ota_announce_step();
        return;
    }

    /* 阶段 1：先收元数据（size + crc32）—— 收到并校验通过后才起 bin 段 */
    if (s_meta_phase != 0u) {
        int r = ota_src_uart_meta_poll(&s_src);

        if (r < 0) {
            ERR_LOG("[OTA] meta recv FAILED (host not metadata-first?)");
            s_meta_phase = 0u;
        } else if (r > 0) {
            ota_src_uart_meta_t m;
            ota_pre_erase_t     pe;

            s_meta_phase = 0u;
            ota_src_uart_meta_get(&s_src, &m);
            RTT_LOG("[OTA] meta: size=%u crc32=0x%08X -> start bin",
                    (unsigned)m.size, (unsigned)m.crc32);

            /* ★ 就趁现在擦目标区：此主机还在等 bin 段的握手 'C'，不会灌数据。
             *   扇区擦除要 ~1s，UART 只有 1 帧的缓冲（1088B），等主机开灌再擦
             *   必然丢字节 → YMODEM 失步 → 永久 NAK（实测 rc=-9）。
             *   流程的 ERASE 阶段会核对这里登记的区间并跳过，于是灌数据期间
             *   只剩毫秒级的页写入。 */
            if (ota_flow_pre_erase(ota_app_self_slot(&s_ota), m.size, &pe) == OTA_OK) {
                RTT_LOG("[OTA] pre-erase ok: slot %u off=0x%X span=%u",
                        (unsigned)pe.slot, (unsigned)pe.off, (unsigned)pe.span);
            } else {
                ERR_LOG("[OTA] pre-erase FAILED -> erase will block during stream");
            }
            if (ota_app_start(&s_ota, ota_src_uart_source(&s_src),
                              s_buf, sizeof(s_buf)) != OTA_OK) {
                ERR_LOG("[OTA] start FAILED");
            }
        }
        return;
    }

    /* 触发：下载口收到 'U'（与 PC 端 tools/fw-ota-ymodem.py 的约定一致）。
     * 顺序要紧：取包 -> **先释放** -> 才允许 send（驱动被锁时 send 返回 -3 拒发）。 */
    if (uart_drv_available(&s_uart) != 0u) {
        uint16_t len  = 0u;
        uint8_t *p    = uart_drv_get_packet(&s_uart, &len);
        uint8_t  trig = 0u;

        if (p != NULL) {
            if (len > 0u && p[0] == (uint8_t)APP_OTA_TRIGGER) {
                trig = 1u;
            }
            uart_drv_release_packet(&s_uart);       /* 取完就还，驱动才能重新武装 RX */
        }
        if (trig != 0u) {
            RTT_LOG("[OTA] trigger '%c' -> announce target slot", APP_OTA_TRIGGER);
            s_announce     = 1u;
            s_announce_try = 0u;
        }
    }
}
