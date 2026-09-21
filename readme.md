# myota-stm32

STM32F407ZGT6（1 MB Flash）双槽 OTA 工程。
两个 CubeMX 子工程 + 一套设计文档，固件基于 [mystm32-sdk](https://github.com/zvozve/mysdk-stm32.git)（拉取到各子工程 `MySDK/`）。

```
app/          APP（TP_MDC.ioc）：业务 + OTA 下载侧（YMODEM 接收、写非运行槽）
bootloader/   BL （TP_MDC_BL.ioc）：激活 / 校验 / 跳转 / 回滚，不做下载
doc/          2026-08 方案设计定稿（01~06：分区、校验、跳转、架构）
```

## Flash 分区（真相源：`app/User/Src/ota_areas.c`）

| 区 | 地址 | 大小 | 说明 |
|---|---|---|---|
| BL  | `0x08000000` | 32K  | Bootloader |
| CFG | `0x08008000` | 32K  | 状态区，两半区乒乓写（各 16K = 一个扇区） |
| SlotA | `0x08010000` | 448K | 运行槽 A |
| SlotB | `0x08080000` | 512K | 运行槽 B |

## 当前 OTA 模式与逻辑

- **A/B 双槽 + 指针切换（SWITCH）**：新固件直接写**非运行槽**，无搬运；旧槽内容不被覆盖，回滚 = 改一个指针。
- **下载在 APP 侧**：USART6（PC6/PC7 @115200）YMODEM，收到 `'U'` 触发。
- **raw `.bin` 直传 + 元数据优先**：先发 8 字节元数据（size + crc32），设备提前判空间、收完按期望 CRC32 校验（CRC-32/ISO-HDLC，与 PC 端 `zlib.crc32` 一致），不再依赖自创的 `.otapkg`。
- **零交互挑槽**：设备回报一行 `#OTA target=A|B`（自动避开运行槽），主机在工程根找「三分片家族」对应那片发送——不选文件、不选分区、不写死地址。
- **三分片构建**：一次 build 出 `TP_MDC.bin`（base，无 BL 可独立跑）+ `TP_MDC_A.bin` + `TP_MDC_B.bin`；槽 `.ld` 由 SDK 的 `ota_slots.cmake` 在 configure 时派生到 `.bin` 同目录，根目录基版 `.ld` 只读。
- **BL 判据 = CFG 状态标记**（`pending_action/trial_slot/boot_try`），不比版本号：激活时读回目标槽重算 CRC → 向量表合法性检查 → 跳转；APP 自检通过后 confirm 结束试运行，`boot_try` 超限自动回滚。
- **校验链路**：逐包 CRC16 → 内存 CRC → 读回 flash CRC → BL 激活复算 → 向量表检查 → 试运行。

## 日常操作（VS Code 任务）

`Build` / `SDK PULL` / `SDK Check OOP`（OOP 红线 + 行尾体检）/ `Flash`（base）/ `Flash Slot-A` / `Flash Slot-B` / `OTA YMODEM`（不给文件即全自动）。地址一律从 `.ld` 解析，任务里不出现任何地址。

## 维护记录

| 日期 | 记录 |
|---|---|
| 2026-08 | 方案设计定稿（`doc/01~06`）：A/B 指针切换取代搬运、5 道校验、APP 自检 confirm 回滚 |
| 2026-09-18 | 接入 `mystm32-sdk`（`MySDK/` 模式）；`ota_core / ota_src_uart / ymodem / bootloader` 落地，分区表定稿 |
| 2026-09-20 | raw bin + 元数据优先；三分片构建 + SDK 侧槽 `.ld` 自动生成；设备自报槽位 + 主机自动挑镜像，实机验证通过（CRC 交叉比对 + flash 读回逐字节校验）；拷出为独立仓库 |

深入阅读：`doc/01-总体方案.md`（决策）、`doc/02`（分区与链接）、`doc/04`（跳转与踩坑）、`app/readme.md`（工程侧细节）。
