# verify-ws2812b-demo

基于 RT-Thread 5.0.0 + STM32F103RET6 的 WS2812B RGB 灯带驱动，使用 **TIM PWM + DMA** 方案。

```bash
hardware: STM32F103RET6 最小系统板
platform: RT-Thread 5.0.0
驱动方式: TIM3 CH3 PWM + DMA1 Channel2 (CIRCULAR 模式)
```

---

## 一、硬件接口

| 接口 | 引脚 | 说明 |
|------|------|------|
| WS2812B DIN | PA0 (TIM3_CH3) | PWM 输出，3.3V 直连 |
| VDD | 5V | 灯带供电 |
| GND | GND | 共地 |

铝基板预留 MOSI 数据引脚、VDD、GND 接口。

![铝基板](./images/ws2812b_images1.jpg)

---

## 二、驱动原理

### 2.1 PWM + DMA 方案

WS2812B 协议要求在 800kHz 下发送 GRB 数据，每个 bit 的高电平时间区分 0/1：

| Bit | 高电平时间 | 低电平时间 | 72MHz 下 PWM tick |
|-----|-----------|-----------|-------------------|
| 0   | ~0.4μs    | ~0.85μs   | 29 / 90 (PWM_HIGH_0) |
| 1   | ~0.8μs    | ~0.45μs   | 58 / 90 (PWM_HIGH_1) |

- TIM3 ARR = 89 → 周期 90 tick = 1.25μs = 800kHz
- DMA 以 HalfWord（16bit）传输 PWM 占空比值
- Circular 模式 + 双缓冲（HT/TC 中断），实现无缝连续输出

### 2.2 数据流

```
用户调用 ws2812b_set_color() / ws2812b_set_all()
    ↓ 更新 leds_color_data[] 缓冲
ws2812b_update()
    ↓ 清空 DMA 缓冲区 → 启动 DMA
DMA 循环传输 → HT/TC 中断触发 update_sequence()
    ↓ 填充下一 half 缓冲区（LED 数据 / 复位 0）
所有 LED 发送完毕 + 复位时间满足
    ↓ HAL_TIM_PWM_Stop_DMA() → 释放信号量
```

---

## 三、CubeMX 配置

打开 CubeMX，按以下步骤配置 TIM3 和 DMA：

### 3.1 TIM3 配置

1. **Clock Source**: Internal Clock
2. **Channel 3**: PWM Generation CH3
3. **Prescaler (PSC)**: 0 （不分频，72MHz 直接进 TIM）
4. **Counter Period (ARR)**: 89 （周期 90 tick = 800kHz）
5. **Pulse (CCR3)**: 0 （初始占空比 0）
6. **CH3 Polarity**: High

![TIM3配置](./images/ws2812b_images3.png)

### 3.2 DMA 配置

1. 在 TIM3 的 **DMA Settings** 标签页添加 DMA 请求
2. **DMA Request**: `TIM3_CH3`
3. **Direction**: Memory → Peripheral
4. **Mode**: **Circular**（必须，双缓冲依赖此模式）
5. **Data Width**: Half Word（16bit，匹配 PWM 占空比寄存器）

![DMA配置](./images/ws2812b_images2.png)

### 3.3 NVIC 配置

- 使能 `DMA1 Channel2` 全局中断
- 优先级保持默认或适当调高

---

## 四、代码结构

```
applications/
├── bsp_ws2812b.h          # 宏定义 + API 声明
├── bsp_ws2812b.c          # 驱动核心（初始化、DMA、中断、灯效）
└── bsp_ws2812b_task.c     # RT-Thread 线程创建（INIT_APP_EXPORT 自动初始化）
```

### 4.1 关键宏定义

```c
#define LED_COUNT       30          // LED 数量
#define PWM_PERIOD      89          // TIM 周期值 (ARR=89, 72MHz/90≈800kHz)
#define PWM_HIGH_0      29          // '0' 高电平 tick ≈0.403μs
#define PWM_HIGH_1      58          // '1' 高电平 tick ≈0.806μs
#define RESET_PRE_MIN   50          // 复位前最小周期数 (>50μs)
#define RESET_POST_MIN  50          // 复位后最小周期数 (>50μs)
#define LEDS_PER_DMA_IRQ 8          // 每个 DMA 中断处理的 LED 数
```

### 4.2 API

```c
void ws2812b_init(void);                                    // 初始化驱动
void ws2812b_set_color(uint16_t index, uint8_t g, uint8_t r, uint8_t b);  // 设置单颗 LED
void ws2812b_set_all(uint8_t g, uint8_t r, uint8_t b);      // 设置所有 LED
rt_err_t ws2812b_update(void);                              // 启动 DMA 传输（非阻塞）
void ws2812b_demo_effects(void);                            // 演示灯效
```

---

## 五、常见踩坑记录

### ⚡ 坑 1：在 RT-Thread 线程中调用 `HAL_TIM_PWM_Start()` 直接死机

**现象**：Hard Fault / BusFault IMPRECISERR，大量寄存器值为 `0xdeadbeef`。

**原因**：`HAL_TIM_PWM_Start()` 内部操作 TIM3 寄存器（使能 CC3 输出），在 RT-Thread 线程（优先级 9）中执行时，DMA 配置和 TIM 状态未完全同步，写缓冲区（write buffer）导致异步总线错误，最终升级为 Hard Fault。

**正确做法**：
- `ws2812b_init()` 中**只做时钟使能 + 信号量创建 + NVIC 配置**
- **不要**在 init 中调用 `HAL_TIM_PWM_Start()` 或 `HAL_TIM_PWM_Start_DMA()`
- DMA 启动留给 `ws2812b_update()` 在需要时调用

```c
void ws2812b_init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    // ... 创建信号量、配置 NVIC
    // ❌ HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);  // 不要在这里启动
    // ❌ HAL_TIM_PWM_Start_DMA(...)                  // 也不行
}
```

### ⚡ 坑 2：`HAL_TIM_PWM_PulseFinishedCallback` 与手动 IRQHandler 冲突

**现象**：DMA 数据传输异常，灯带闪烁或不亮。

**原因**：STM32F1 的 TIM3 DMA 使用 DMA1 Channel 2。`HAL_TIM_PWM_PulseFinishedCallback` 是 HAL 内部回调（在 `HAL_DMA_IRQHandler` 中被调用），会停止 DMA 并释放信号量。同时手动的 `DMA1_Channel2_IRQHandler` 又在检查 HT/TC 标志并调用 `update_sequence()`。两者竞争同一 DMA 传输，导致总线错误。

**正确做法**：
- **删除** `HAL_TIM_PWM_PulseFinishedCallback`
- **删除** `DMA1_Channel2_IRQHandler` 中的 `HAL_DMA_IRQHandler()` 调用
- 仅保留手动标志检查 + `update_sequence()` 逻辑

```c
void DMA1_Channel2_IRQHandler(void)
{
    // ❌ HAL_DMA_IRQHandler(&hdma_tim3_ch3);  // 不要调用，它会清除 HT/TC 标志

    if (__HAL_DMA_GET_FLAG(&hdma_tim3_ch3, DMA_FLAG_HT2))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_tim3_ch3, DMA_FLAG_HT2);
        update_sequence(0);
    }
    if (__HAL_DMA_GET_FLAG(&hdma_tim3_ch3, DMA_FLAG_TC2))
    {
        __HAL_DMA_CLEAR_FLAG(&hdma_tim3_ch3, DMA_FLAG_TC2);
        update_sequence(1);
    }
}
```

### ⚡ 坑 3：`HAL_DMA_Init()` 运行中重复调用

**现象**：第一次 DMA 正常，第二次调用 `ws2812b_update()` 后死机。

**原因**：`HAL_DMA_Init()` 会重新配置 DMA 寄存器，与正在运行的 DMA 传输冲突。

**正确做法**：
- DMA 在 CubeMX 启动时通过 `MX_DMA_Init()` 初始化一次
- `ws2812b_init()` 中**不调用** `HAL_DMA_Init()`
- `ws2812b_update()` 中也**不调用**
- CIRCULAR 模式在 CubeMX 中配置，或仅在 `ws2812b_init()` 中赋值 `Init.Mode`（不调用 Init）

### ⚡ 坑 4：线程栈溢出

**现象**：Hard Fault，SP 指向栈底附近。

**原因**：`ws2812b_demo_effects()` + DMA 操作需要较多栈空间，4096 字节不够。

**正确做法**：线程栈至少 **8192 字节**。

```c
rt_thread_create("WS2812B_Thread", WS2812B_Thread_entry, RT_NULL, 8192, 9, 100);
```

### ⚡ 坑 5：复位时间不足导致灯带颜色混乱

**现象**：灯带显示颜色偏移，部分 LED 不亮。

**原因**：WS2812B 要求 ≥50μs 低电平复位。`RESET_PRE_MIN` 和 `RESET_POST_MIN` 过小（如 10/8 个周期 = 12.5μs/10μs）不满足协议要求。

**正确做法**：`RESET_PRE_MIN` 和 `RESET_POST_MIN` 至少 **50**（50 × 1.25μs = 62.5μs）。

### ⚡ 坑 6：缓冲区对齐问题

**现象**：BusFault IMPRECISERR，偶发。

**原因**：DMA 要求缓冲区地址与传输宽度对齐。`uint16_t` 缓冲区需要 2 字节对齐，但某些编译器/链接器可能不保证。

**正确做法**：使用 `__attribute__((aligned(4)))` 确保 4 字节对齐。

```c
__attribute__((aligned(4))) uint16_t ws2812_buffer[DMA_BUFF_LEN] = {0};
```

### ⚡ 坑 7：信号量永久等待导致线程死锁

**现象**：系统挂起，WS2812B 线程不再响应。

**原因**：`ws2812b_demo_effects()` 中使用 `rt_sem_take(dma_complete_sem, RT_WAITING_FOREVER)`，若 DMA 异常停止，信号量永远不会释放。

**正确做法**：使用超时机制。

```c
if (rt_sem_take(dma_complete_sem, 100) != RT_EOK)
{
    LOG_W("WS2812B DMA 超时，跳过本次更新");
}
```

---

## 六、调试建议

1. **先验证 TIM 输出**：不接灯带，用示波器/逻辑分析仪测量 PA0，确认 800kHz PWM 波形
2. **验证 PWM 占空比**：`PWM_HIGH_0` 应输出 ~0.4μs 高电平，`PWM_HIGH_1` 应输出 ~0.8μs
3. **验证复位信号**：发送完毕后应有 ≥50μs 低电平
4. **串口日志**：开启 `LOG_D` 观察 DMA 启动/完成流程
5. **灯效顺序**：建议先测纯色（红/绿/蓝），再测流水灯

---

## 七、参考文档

- [调试记录](./docs/Debug.md) — 历次 Hard Fault 排查过程与修复方案
- [问题清单](./docs/table.csv) — 代码审查发现的问题汇总

---

## License

Apache-2.0
