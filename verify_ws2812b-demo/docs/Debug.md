# RT-Thread + STM32F1 WS2812B PWM+DMA 驱动问题总结与修复建议

**文档标签**：`#rt-thread` `#ws2812b` `#hardfault` `#pwm-dma` `#stm32f1`  
**适用版本**：RT-Thread 5.0.0 + STM32F103RE  
**创建日期**：2026-04-14  
**问题等级**：严重（初始化阶段触发 Hard Fault + BusFault IMPRECISERR）

## 1. 故障现象

- 线程 `WS2812B_Thread`（优先级 9）在初始化阶段（`ws2812b_init()`）触发 **Hard Fault**
- 随后出现 **BusFault (IMPRECISERR)**
- 寄存器特征：
  - 大量寄存器值为 `0xdeadbeef`（典型栈/内存破坏标志）
  - PC 指向 `0x0800881a`（位于 `ws2812b_init()` 或 DMA 启动附近）
  - 异常发生在**线程模式**

## 2. 核心问题分析

### 2.1 最严重的问题（直接导致 Hard Fault）

1. **DMA 中断处理严重冲突**
   - 同时存在 `HAL_TIM_PWM_PulseFinishedCallback` 和手动 `DMA1_Channel2_IRQHandler`（处理 HT/TC）
   - HAL PWM DMA 机制与手动清 DMA 标志 + `update_sequence()` 冲突，导致总线错误（IMPRECISERR）

2. **DMA 重复初始化**
   - 在 `ws2812b_update()` 中每次都调用 `HAL_DMA_Init(&hdma_tim3_ch3)`
   - 与 CubeMX 在 `board.c` / `stm32f1xx_hal_msp.c` 中已完成的初始化冲突

3. **初始化顺序与时钟问题**
   - `ws2812b_init()` 未确保 TIM3 和 DMA 时钟已稳定开启
   - 没有对 PWM 设备句柄进行有效性检查

### 2.2 其他重要问题

- **线程栈空间不足**：当前仅 4096 字节，在执行 `ws2812b_demo_effects()` + DMA 操作时容易溢出
- **双缓冲逻辑复杂且存在 Bug**：
  - `led_cycles_cnt` 计数和复位（RESET_PRE_MIN / RESET_POST_MIN）逻辑容易越界
  - Circular 模式下停止 DMA 的时机处理不当
- **重入风险**：`ws2812b_demo_effects()` 频繁调用 `ws2812b_update()`，`is_updating` 保护不够完善
- **PWM 参数精度**：`PWM_HIGH_0=32`、`PWM_HIGH_1=64` 在 72MHz 下占空比不够精确
- **自动初始化冲突**：`bsp_ws2812b_task.c` 使用 `INIT_APP_EXPORT`，而 `main.c` 中曾注释掉手动初始化

## 3. 修复优先级建议

### 必须立即修复（高优先级）
1. **增大线程栈** → 改为 8192 字节（8KB）
2. **解决 DMA 中断冲突** → 优先保留手动 `DMA1_Channel2_IRQHandler`，注释或移除 PulseFinishedCallback
3. **移除运行时重复的 `HAL_DMA_Init()`**
4. **在 `ws2812b_init()` 最前面确保时钟使能**
5. **添加指针/句柄有效性检查**

### 中优先级优化
- 简化 DMA 更新逻辑（推荐先改成**单缓冲阻塞式**进行调试）
- 为关键变量添加 `volatile` 和 `ALIGN(4)`
- 优化 PWM 高低电平参数（建议 HIGH_0 ≈ 29~35，HIGH_1 ≈ 58~70）
- 使用信号量正确等待 DMA 完成

### 长期推荐方案
- 改为成熟的单次填充 + DMA 一次传输方案（更稳定）
- 考虑切换到 **SPI + DMA** 驱动 WS2812B（STM32F1 上更可靠）
- 将 WS2812B 封装成标准 RT-Thread 设备驱动（`rt_device`）

## 4. 快速验证步骤

1. 将 WS2812B 线程栈改为 8192 字节并重新编译
2. 在 `ws2812b_init()` 最开始添加：
   ```c
   __HAL_RCC_TIM3_CLK_ENABLE();
   __HAL_RCC_DMA1_CLK_ENABLE();