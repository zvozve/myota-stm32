#include "app_main.h"

// 板级绑定（唯一允许引用 MX 符号处：CPU_STA_*/ETH_RST_*/&huart/&hiwdg）
#include "board_cfg.h"

// BSP（SDK OOP 封装）
#include "oop_dwt.h"

// OTA（分区表 + APP 外壳 + 串口通道）
#include "app_ota.h"

// Hardware
#include "heart_beat.h"
#include "lan8720a_reset.h"

// Middleware
#include "SEGGER_RTT_Log.h"

// Protocols



// ============================================
// RTOS支持
// 开关唯一来源 = User/board_cfg.h 的 BOARD_USE_RTOS（RTOS(1)/裸机(0)）。
// 不要在这里再私有定义：SDK 的 chip 层（oop_dwt 时基/延时）读的是同一个开关，
// app 与 SDK 若不一致会出现「任务在跑 RTOS 时基、SDK 却忙等」的混合状态。
// 注意用 #if 而不是 #ifdef —— #ifdef 对 -DAPP_USE_RTOS=0 无效。
// ============================================
#define APP_USE_RTOS    BOARD_USE_RTOS

#if APP_USE_RTOS
    #include "FreeRTOS.h"
    #include "task.h"
    #include "cmsis_os.h"

    // 入口任务句柄
    static TaskHandle_t xAppTaskHandle = NULL;

    // 入口任务函数
    static void vAppTask(void *pvParameters);
#endif

// ============================================
// 裸机模式初始化
// ============================================
#if !APP_USE_RTOS
static void App_BareMetal_Init(void) {
    heart_beat_init(BOARD_HEART_LED_PORT, BOARD_HEART_LED_PIN, BOARD_HEART_IWDG);
    App_BareMetal_Loop();
}

static void App_BareMetal_Loop(void) {
    static uint32_t last_heartbeat = 0;

    while (1) {
        uint32_t now = oop_GetCycleCount();

        // 心跳处理（500ms）
        if (oop_IsTimeout(last_heartbeat, 500000)) {
            last_heartbeat = now;
            heart_beat_run();
        }

        oop_DelayUS(1000);
    }
}
#endif

// ============================================
// RTOS入口任务
// ============================================
#if APP_USE_RTOS

static void vAppTask(void *pvParameters) {
    heart_beat_init(BOARD_HEART_LED_PORT, BOARD_HEART_LED_PIN, BOARD_HEART_IWDG);

    // RTOS循环
    while (1) {
        static uint32_t last_feed_time = 0;
        uint32_t current_time = xTaskGetTickCount();
        if ((current_time - last_feed_time) >= pdMS_TO_TICKS(200)) {
            last_feed_time = current_time;
            heart_beat_run();
        }

        app_ota_poll();     /* OTA：'U' 触发下载 / 下载中推进 / 完成时重启进 BL */

        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms周期
    }
}

static void App_RTOS_CreateTask(void) {
    xTaskCreate(
        vAppTask,               // 任务函数
        "AppTask",              // 任务名称
        1024,                   // 任务栈大小（LwIP + RTT 调用链较深，加大防溢出）
        NULL,                   // 任务参数
        osPriorityNormal,       // 任务优先级
        &xAppTaskHandle        // 任务句柄
    );
}

#endif

void App_Init(void) {
    // 基础硬件初始化
    ETH_RST_Init(BOARD_ETH_RST_PORT, BOARD_ETH_RST_PIN);
    oop_InitDWT();
    APP_LOG("APP_FW_VERSION: %s, APP_PUBLISH_DATE: %s", APP_FW_VERSION, APP_PUBLISH_DATE);
    ETH_RST_Execute();

    // OTA：分区表 + 下载口（USART6）。失败只影响升级能力，业务照常跑
    (void)app_ota_setup();

#if APP_USE_RTOS
    // RTOS模式
    App_RTOS_CreateTask();
#else
    // 裸机模式
    App_BareMetal_Init();

#endif
}

// ============================================
// 应用循环（兼容接口）
// ============================================
void App_Loop(void) {
#if APP_USE_RTOS
    // RTOS模式：由调度器管理，不应到达
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#else
    // 裸机模式：由App_Init调用，此处不会执行
    // 保留此函数作为兼容接口
    App_BareMetal_Loop();
#endif
}
