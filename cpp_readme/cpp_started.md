# Moteus 使用笔记

## macOS 启动 tview

```bash
python3 -m moteus_gui.tview --devices=1 --fdcanusb /dev/cu.usbmodemAAC8D32F1
```

> `AAC8D32F1` 是设备序列号，按实际替换。

---

## Moteus 项目总结

### 是什么

mjbots Robotic Systems 开发的全开源无刷伺服控制器系统（Apache 2.0），从 PCB 设计到固件到客户端库全部开源，同时在 mjbots.com 销售成品板。目标场景是高性能机器人关节。

### 架构三层

```
[硬件 PCB]  →  [STM32G4 固件]  →  [客户端库: Python / C++ / Rust]
               baremetal, no RTOS     同一套寄存器协议
```

- **固件** (`fw/`): 纯裸机 C++，mbed-OS 仅作 HAL。主循环 poll 毫秒定时器；实时控制在硬件 ISR 中完成，无调度器开销。启动链路：`Fdcan → FDCanMicroServer → MultiTransportDatagramServer → MicroServer → MoteusController → BldcServo`。
- **控制环** (`bldc_servo_control.h`, 1815 行): 标准 FOC——三相电流 ADC → Clarke → Park → DQ 电流 PI → 逆 Park → SVM。全部放在 CCM SRAM（零等待状态）；sin/cos 由 STM32G4 的 CORDIC 硬件协处理器计算。
- **通信协议**: 自定义 multiplex 寄存器映射（v5），寄存器地址 uint16，数值精度可按需选 INT8/INT16/F32——每帧省字节即省 CAN-FD 带宽。仲裁 ID 编码源/目标 node ID（1–126），同一 CAN 总线可同时跑控制命令 + 文本诊断 tunnel。

### 硬件型号

| | r4.11 | c1 | n1 | x1 |
|---|---|---|---|---|
| 输入电压 | 10–44V | 10–51V | 10–54V | 10–54V |
| 峰值功率 | 900W | 250W | 2kW | 1.3kW |
| 峰值相电流 | 100A | 20A | 100A | 120A |
| 重量 | 14.2g | 8.9g | 14.6g | 23.8g |
| PWM 频率 | 30–60kHz | 15kHz | 30kHz | 30kHz |

同一份固件二进制，运行时 `DetectMoteusFamily()` 自动适配板型差异（电流采样电阻、电压限制、TDC 补偿等）。

### 客户端生态

- **Python** (`lib/python/moteus/`): asyncio 原生，`Controller` 封装所有模式。`QueryResolution`/`PositionResolution` 描述符可按调用控制寄存器精度。Transport: fdcanusb（USB CDC）、python-can（SocketCAN 等）。
- **C++** (`lib/cpp/mjbots/moteus/`): 纯头文件，支持阻塞和回调异步两种 API。`moteus_protocol.h` 可单独嵌入使用。
- **Rust** (`lib/rust/`): 拆为 `moteus-protocol`（no_std，可嵌入）+ `moteus`（std，transport）。`BlockingController::new(id)` 自动发现 transport；支持 fdcanusb、SocketCAN、TTL UART。

### 值得关注的技术点

1. **Watchdog 安全模型**: `kPositionTimeout` 是独立的受控降级状态，而非简单报错——必须主动发 stop 才能退出，防止断连后失控。
2. **256 点齿槽力矩补偿表**: 每个电气周期 256 采样的前馈表，由标定工具生成存入 flash，可显著降低低速转矩脉动。
3. **传输复用**: 同一根 CAN-FD 总线既跑电机指令，也跑 multiplex tunnel（虚拟串口），`tview.py` 通过 tunnel 读取实时遥测和文本控制台，无需额外调试接口。
4. **1.0.0 首次语义化版本**: 2026-05-28 发布，固件 + Python + C++ 同步打标，寄存器映射版本（v5）存入固件，客户端可检测不匹配。

### 构建体系

Bazel 唯一构建工具，必须用仓库自带的 `tools/bazel` 包装器（版本锁定）。CMake 仅作 C++ 客户端库的兼容 shim。CI 跑两趟测试：普通 host 构建 + UBSan 构建，再加一次 firmware 交叉编译。

---

## 构建方式 & 环境要求

### macOS 限制

`tools/bazel` 包装脚本写死下载 Linux x86_64 的 Bazel 二进制，**macOS 上无法直接使用**。官方支持平台是 Linux x86_64。

### Linux 环境要求

| 项目 | 要求 |
|---|---|
| OS | Ubuntu（推荐 22.04/24.04） |
| Python | **3.7–3.x**（`requires-python = ">=3.7, <4"`），建议用系统 Python 3.12 配合 apt 包 |
| 编译器 | Bazel 自动下载 LLVM 20.1.8，无需手动装 GCC/Clang |
| Bazel | 由 `tools/bazel` 自动下载 7.4.1，**不要用系统 bazel** |
| curl | 供 `tools/bazel` 下载 Bazel 本体 |

安装 Ubuntu 系统包：

```bash
sudo apt update
sudo apt install -y python3-build python3-can python3-serial python3-setuptools \
    python3-pyelftools python3-qtpy python3-wheel python3-importlib-metadata \
    python3-scipy python3-usb mypy nodejs
```

C++ 测试额外需要：

```bash
wget http://security.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.3-2ubuntu0.1_amd64.deb
sudo apt install -y ./libtinfo5_6.3-2ubuntu0.1_amd64.deb
```

避免 Python 版本冲突：

```bash
sudo mv /usr/local/bin/python3 /usr/local/bin/python3.bak
```

### 三类构建目标

```bash
# 编译固件（交叉编译 STM32G4）
tools/bazel build --config=target //:target

# 运行 host 单元测试（快速，默认 small size）
tools/bazel test --config=host //:host

# 完整测试套件（含仿真回归 + UBSan）
./travis-ci.sh
```

单独跑某个测试：

```bash
tools/bazel test --config=host //fw:test         # 固件 C++ 单元测试
tools/bazel test --config=host //utils:test      # 工具测试
tools/bazel test --config=host //lib/python:host # Python 库测试
tools/bazel test //lib/rust:host                 # Rust 库测试
```

### macOS 上的替代方案

- **直接用 Python 库**（无需构建）：`pip install moteus`，或直接运行 `utils/moteus_tool.py` / `utils/tview.py`
- **构建固件**：需在 Linux 机器或 Docker 容器内进行
