# For macs run tveiw #

```
python3 -m moteus_gui.tview --devices=1 --fdcanusb /dev/cu.usbmodemAAC8D32F1
```
# (AAC8D32F1: Serial Numember) #

⏺ Moteus 项目总结

  是什么

  mjbots Robotic Systems 开发的全开源无刷伺服控制器系统（Apache 2.0），从 PCB 设计到固件到客户端库全部开源，同时在 mjbots.com 销售成品板。目标场景是高性能机器人关节。

  ---
  架构三层

  [硬件 PCB]  →  [STM32G4 固件]  →  [客户端库: Python / C++ / Rust]
                 baremetal, no RTOS     同一套寄存器协议

  固件 (fw/): 纯裸机 C++，mbed-OS 仅作 HAL。主循环 poll 毫秒定时器；实时控制在硬件 ISR 中完成，无调度器开销。启动链路：Fdcan → FDCanMicroServer → MultiTransportDatagramServer → MicroServer →
  MoteusController → BldcServo。

  控制环 (bldc_servo_control.h, 1815 行): 标准 FOC——三相电流 ADC → Clarke → Park → DQ 电流 PI → 逆 Park → SVM。全部放在 CCM SRAM（零等待状态）；sin/cos 由 STM32G4 的 CORDIC 硬件协处理器计算。

  通信协议: 自定义 multiplex 寄存器映射（v5），寄存器地址 uint16，数值精度可按需选 INT8/INT16/F32——每帧省字节即省 CAN-FD 带宽。仲裁 ID 编码源/目标 node ID（1–126），同一 CAN 总线可同时跑控制命令 + 文本诊断
  tunnel。

  ---
  硬件型号

  ┌────────────┬──────────┬────────┬────────┬────────┐
  │            │  r4.11   │   c1   │   n1   │   x1   │
  ├────────────┼──────────┼────────┼────────┼────────┤
  │ 输入电压   │ 10–44V   │ 10–51V │ 10–54V │ 10–54V │
  ├────────────┼──────────┼────────┼────────┼────────┤
  │ 峰值功率   │ 900W     │ 250W   │ 2kW    │ 1.3kW  │
  ├────────────┼──────────┼────────┼────────┼────────┤
  │ 峰值相电流 │ 100A     │ 20A    │ 100A   │ 120A   │
  ├────────────┼──────────┼────────┼────────┼────────┤
  │ 重量       │ 14.2g    │ 8.9g   │ 14.6g  │ 23.8g  │
  ├────────────┼──────────┼────────┼────────┼────────┤
  │ PWM 频率   │ 30–60kHz │ 15kHz  │ 30kHz  │ 30kHz  │
  └────────────┴──────────┴────────┴────────┴────────┘

  同一份固件二进制，运行时 DetectMoteusFamily() 自动适配板型差异（电流采样电阻、电压限制、TDC 补偿等）。

  ---
  客户端生态

  - Python (lib/python/moteus/): asyncio 原生，Controller 封装所有模式。QueryResolution/PositionResolution 描述符可按调用控制寄存器精度。Transport: fdcanusb（USB CDC）、python-can（SocketCAN 等）。
  - C++ (lib/cpp/mjbots/moteus/): 纯头文件，支持阻塞和回调异步两种 API。moteus_protocol.h 可单独嵌入使用。
  - Rust (lib/rust/): 拆为 moteus-protocol（no_std，可嵌入）+ moteus（std，transport）。BlockingController::new(id) 自动发现 transport；支持 fdcanusb、SocketCAN、TTL UART。

  ---
  几个值得关注的技术点

  1. 有意设计的 watchdog 安全模型: kPositionTimeout 是独立的受控降级状态，而非简单报错——必须主动发 stop 才能退出，防止断连后失控。
  2. 256 点齿槽力矩补偿表: 每个电气周期 256 采样的前馈表，由标定工具生成存入 flash，可显著降低低速转矩脉动。
  3. 传输复用: 同一根 CAN-FD 总线既跑电机指令，也跑 multiplex tunnel（虚拟串口），tview.py 通过 tunnel 读取实时遥测和文本控制台，无需额外调试接口。
  4. 1.0.0 首次语义化版本: 2026-05-28 发布，固件 + Python + C++ 同步打标，寄存器映射版本（v5）存入固件，客户端可检测不匹配。

  ---
  构建体系

  Bazel 唯一构建工具，必须用仓库自带的 tools/bazel 包装器（版本锁定）。CMake 仅作 C++ 客户端库的兼容 shim。CI 跑两趟测试：普通 host 构建 + UBSan 构建，再加一次 firmware 交叉编译。




