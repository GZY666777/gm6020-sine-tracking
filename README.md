# GM6020 正弦速度 / 位置跟踪

基于 **RoboMaster C 型开发板（STM32F407IGHx）+ GM6020 + J-Link** 的单电机控制作业。

程序让 GM6020 依次跟踪两条自定义正弦曲线：

| 模式 | 曲线 | 默认参数 |
| --- | --- | --- |
| 速度跟踪 | `rpm_ref = A·sin(2πft)` | `A = 200 rpm`，`f = 0.2 Hz` |
| 位置跟踪 | `deg_ref = zero + A·sin(2πft)` | `A = ±60°`，`f = 0.1 Hz` |

上电后默认进入 **自动演示**：10 s 速度正弦 → 10 s 位置正弦 → 循环。两条曲线都在零点切换，切换无冲击。

保护机制包括：

- **通信看门狗**：100 ms 收不到 GM6020 反馈 → 立即输出 0 并锁存故障；
- **独立看门狗 IWDG**：250 ms，由 1 kHz 控制步刷新，固件跑飞自动复位；
- **过温、位置越限、CAN bus-off、连续发送失败** 保护；
- 故障后必须确认电机反馈正常、温度正常、转子回到安全范围，才能用 `r` 或按键清除。

---

## 1. 硬件与接线

### 1.1 清单

- RoboMaster C 型开发板（STM32F407IGHx）
- GM6020 电机（默认 ID = 1，1 Mbps）
- J-Link（SWD 调试/下载）
- 24 V 电源（建议 ≥ 5 A，电机和开发板共地）
- USB-TTL 串口模块（可选，用于看波形/发命令）
- CAN 线、杜邦线、电机固定支架

### 1.2 接线

| 信号 | C 板 | GM6020 / 外部 |
| --- | --- | --- |
| CAN_H | CAN1_H | 电机 CAN_H |
| CAN_L | CAN1_L | 电机 CAN_L |
| GND | GND | 电机 GND / 24 V 电源 GND |
| 24 V | 24 V 输入/输出（以丝印为准） | 电机 24 V |
| SWDIO | SWD 接口 SWDIO | J-Link SWDIO（PIN 7） |
| SWCLK | SWD 接口 SWCLK | J-Link SWCLK（PIN 9） |
| GND | SWD GND | J-Link GND（PIN 4） |
| 3V3 | SWD 3V3（仅参考） | J-Link 3V3（PIN 1，可选） |
| UART TX | PA9 | USB-TTL **RX** |
| UART RX | PB7 | USB-TTL **TX** |
| UART GND | GND | USB-TTL GND |

注意：

1. **先共地再上电**。电机和开发板必须共地，否则 CAN 可能直接损坏。
2. C 板 CAN 口是否直接带 24 V 以你手上板子的丝印/原理图为准。**上电前用万用表确认**；不确定时电机 24 V 单独从电源取，只把 CAN_H/CAN_L/GND 接到 C 板。
3. 电机第一次测试请**空载并可靠固定**，不要带云台负载；先用手确认转子能自由转动。
4. CAN 总线两端建议 120 Ω 终端电阻；短线（< 30 cm）连接时通常可以不额外加，但通信不稳时优先检查终端电阻和共地。
5. J-Link 只负责调试，不要用 J-Link 的 3.3 V 给电机或开发板供电。

更完整的接线说明见 [`docs/wiring.md`](docs/wiring.md)。

---

## 2. 软件环境

- Keil MDK-ARM 5（ARMCC 5.06 / AC5）
- STM32F4xx DFP `2.13.0` 或更新
- SEGGER J-Link 驱动
- 工程文件：[`MDK-ARM/GM6020_Sine.uvprojx`](MDK-ARM/GM6020_Sine.uvprojx)
- 可选：CMake + `arm-none-eabi-gcc`，见 [`docs/build-gcc.md`](docs/build-gcc.md)

> 当前验证状态：**完整 GCC 编译 + 链接已通过**，FLASH 52,692 B（5.03%）、RAM 4,936 B（3.77%）；硬件实测和参数整定待装置到位后进行。

### 2.1 打开、编译、下载

1. 双击打开 `MDK-ARM/GM6020_Sine.uvprojx`。
2. `Project → Options for Target → Debug`：
   - 选择 **J-LINK / J-TRACE Cortex**；
   - `Settings → Port` 选 **SW**，`Max Clock` 先设 **4 MHz**；
   - 确认能识别到 `STM32F407IG` 的 IDCODE。
3. `Utilities`：
   - 勾选 **Use Debug Driver**（或手动选择 J-LINK）；
   - `Settings → Flash Download` 勾选 **Reset and Run**；
   - 确认有 **STM32F4xx 1MB Flash** 算法。
4. `F7` 编译，`F8` 下载。
5. 串口助手打开 **115200 8N1**，或运行 `tools/plot_serial.py`。

> 如果 Keil 报 `Cannot access target`：检查 SWDIO/SWCLK/GND、板子是否上电、J-Link 是否被其他软件占用；把 Max Clock 降到 1 MHz 再试。

> 本工程已经包含 CubeMX 生成的外设初始化代码。**不要再用 CubeMX 重新生成**，否则可能覆盖 `Src/main.c` 和工程配置。需要改外设时，先备份工程，或只修改 `USER CODE` 区域内的代码。

### 2.2 电机 ID

GM6020 出厂默认 **ID = 1**。如果之前改过 ID：

- 用 RoboMaster Assistant 改回 1；或
- 修改 [`application/app_config.h`](application/app_config.h) 里的 `GM6020_ID`。

驱动按 ID 自动选择控制帧：ID 1–4 用 `0x1FF`，ID 5–7 用 `0x2FF`；反馈帧为 `0x204 + ID`。

### 2.3 没有 Keil 时的备用构建

```powershell
cmake -S . -B C:/temp/gm6020-build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake `
  -DARM_TOOLCHAIN_PATH="D:/path/to/arm-none-eabi/bin" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build C:/temp/gm6020-build -j 8
```

会生成 `.elf/.bin/.hex`。详细说明和 J-Link 命令行下载见 [`docs/build-gcc.md`](docs/build-gcc.md)。

---

## 3. 运行与操作

### 3.1 板载按键 PA0

每按一次循环切换：

```
速度正弦 → 位置正弦 → 停止 → 自动演示 → 速度正弦 ...
```

故障状态下按按键 = 尝试清除故障。

### 3.2 串口命令

| 命令 | 功能 |
| --- | --- |
| `s` | 速度正弦跟踪 |
| `p` | 位置正弦跟踪 |
| `a` | 自动演示（10 s + 10 s 循环） |
| `x` | 停止，输出电压 0 |
| `r` | 清除锁存故障（需反馈正常、温度正常、位置安全） |
| `?` | 打印帮助 |

### 3.3 板载 RGB LED

| 状态 | LED |
| --- | --- |
| 等待电机反馈 | 蓝灯 200 ms 闪烁 |
| 速度模式运行 | 绿灯常亮 |
| 位置模式运行 | 蓝灯常亮 |
| 故障锁存 | 红灯 100 ms 闪烁 |
| 停止 | 全灭 |

### 3.4 上电后的正常现象

1. 蓝灯闪烁约 0.3 s，等待第一帧反馈；
2. 串口打印 `# RUN mode=0 ...`；
3. 电机开始按 0.2 Hz、±200 rpm 正弦转动；
4. 10 s 后切到位置模式，电机按 0.1 Hz、±60° 摆动；
5. 再 10 s 回到速度模式。

如果电机不动、串口没有 `# RUN`：先检查 CAN 接线、电机 24 V、ID 和 1 Mbps 波特率。

---

## 4. 控制原理

### 4.1 速度环

```
rpm_ref ──►(+)──► PI ──►(+)──► 限幅 ──► GM6020 电压指令
            ▲           ▲
            │           └── 前馈：Kff · rpm_ref
          rpm 反馈
```

- 误差单位 rpm，输出单位 GM6020 电压指令（±30000）；
- `Kff = 90 counts/rpm` 是反电动势/转速前馈，让正弦跟踪主要靠前馈，PI 只修残差；
- 积分限幅 + 条件积分抗饱和。

### 4.2 位置环（串级）

```
deg_ref ──►(+)──► P ──►(+)──► 速度环 ──► 电机
            ▲           ▲
            │           └── 速度前馈：d(deg_ref)/dt ÷ 6
          deg 反馈
```

- 外环 P：角度误差（°）→ 速度指令（rpm），限幅 ±250 rpm；
- 角度误差做 `wrap180`，始终走最短路径；
- 进入位置模式时把当前位置记为 `zero`，因此从速度模式切过来不会跳变；
- 内环仍是上面的速度 PI。

### 4.3 1 kHz 实时结构

```
SysTick (1 kHz)
  └─ HAL_IncTick()
  └─ App_ControlStep()
       ├─ 刷新 IWDG
       ├─ 处理模式/清故障命令
       ├─ 正弦轨迹 + 位置环 + 速度环
       ├─ GM6020 电压帧发送
       └─ Safety_Update() → 故障则输出 0

主循环
  ├─ 串口命令
  ├─ 按键
  ├─ 50 Hz CSV 日志
  └─ LED 状态
```

所有 CAN 发送和状态切换都放在 1 kHz 控制步里，避免主循环和中断同时操作 CAN 发送缓冲区。

---

## 5. 参数表

全部参数集中在 [`application/app_config.h`](application/app_config.h)：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `GM6020_ID` | 1 | 电机 ID |
| `MOTOR_SIGN` | +1 | 方向修正；反装时改 -1（角度/转速/输出同时取反） |
| `SINE_SPEED_AMP_RPM` | 200 | 速度正弦幅值 |
| `SINE_SPEED_FREQ_HZ` | 0.2 | 速度正弦频率 |
| `SINE_POS_AMP_DEG` | 60 | 位置正弦幅值 |
| `SINE_POS_FREQ_HZ` | 0.1 | 位置正弦频率 |
| `SPEED_KP / KI / KD` | 80 / 800 / 0 | 速度环 |
| `SPEED_FF_COUNTS_PER_RPM` | 90 | 速度前馈 |
| `POS_KP / KI / KD` | 4 / 0 / 0 | 位置环 |
| `POS_MAX_RPM` | 250 | 位置环输出限幅 |
| `SAFETY_FEEDBACK_TIMEOUT_MS` | 100 | 反馈看门狗 |
| `SAFETY_TEMP_FAULT_C` | 80 | 过温故障 |
| `SAFETY_POS_LIMIT_DEG` | 100 | 位置软限位 |
| `SAFETY_IWDG_RELOAD` | 250 | IWDG 约 250 ms |
| `AUTO_PHASE_S` | 10 | 自动演示每段时长 |

### 5.1 整定顺序

1. **方向检查**：给一个小正电压，确认转速/角度按正方向变化；反了就把 `MOTOR_SIGN` 改成 `-1`。
2. **速度环**：先 `KI = 0`，从小 `KP` 加到手能感到“有劲但不抖”，再加 `KI` 消除稳态误差；`Kff` 最后微调。
3. **位置环**：速度环稳定后再加 `POS_KP`，从 1 开始，出现等幅振荡就减半。
4. 每次改参数只改一个，改完重新下载并记录波形。

---

## 6. 保护与看门狗

| 保护 | 触发条件 | 动作 |
| --- | --- | --- |
| 反馈超时 | 连续 100 ms 无反馈 | 输出 0，锁存 `FEEDBACK_TIMEOUT` |
| IWDG | 1 kHz 控制步停止 > 250 ms | MCU 复位 |
| 过温 | 反馈温度 > 80 °C | 输出 0，锁存 `OVERTEMP` |
| 位置越限 | 位置模式下车角偏离 zero > 100° | 输出 0，锁存 `POS_LIMIT` |
| CAN bus-off | CAN1 进入 bus-off | 输出 0，锁存 `CAN_BUSOFF` |
| 发送失败 | 连续 50 次发送失败 | 输出 0，锁存 `CAN_TX` |

故障锁存后不会自动恢复。串口会打印类似：

```
# FAULT 0x00000001: FEEDBACK_TIMEOUT
```

清除条件（`r` 或按键）：

- 反馈已恢复；
- 温度 < 70 °C；
- 转子回到 zero 的 ±80° 内；
- bus-off 时尝试重启 CAN 控制器。

> 调试时 IWDG 已通过 `DBGMCU_APB1FZ` 设置“内核暂停时冻结”，所以在 Keil 里打断点不会一直被复位。真实运行（不暂停）时看门狗仍然有效。

---

## 7. 波形记录

串口 50 Hz 输出 CSV，字段：

```
ms,state,mode,auto,rpm_ref_x10,rpm_x10,deg_ref_rel_x10,deg_rel_x10,voltage,temp,fault_hex
```

- `state`：0 等待，1 运行，2 故障，3 停止
- `mode`：0 速度，1 位置
- `rpm_*_x10`：实际值 ×10（例如 `2000` = 200.0 rpm）
- `deg_*_rel_x10`：相对 zero 的角度 ×10（例如 `600` = 60.0°）
- `voltage`：电压指令（±30000）
- `fault_hex`：故障位掩码

### 7.1 采集 + 绘图

```bash
pip install -r tools/requirements.txt

# 采集 30 s，保存 CSV
python tools/plot_serial.py --port COM5 --seconds 30 --out run.csv

# 用已有 CSV 画图
python tools/plot_serial.py --csv run.csv --save run.png
```

脚本会画四张图：速度跟踪、位置跟踪、电压指令、跟踪误差。把 `run.png` 和 CSV 一起放进实验报告。

### 7.2 实验记录

报告模板见 [`docs/report_template.md`](docs/report_template.md)，建议至少记录：

- 空载速度正弦：目标 vs 实际、最大跟踪误差；
- 位置正弦：目标 vs 实际、最大误差、是否有超调；
- 断开 CAN / 拔掉电机电源，观察 100 ms 反馈看门狗动作；
- 用手堵转，观察温度/位置保护（注意不要长时间堵转）；
- 故障清除流程。

---

## 8. 目录结构

```
gm6020-sine-tracking/
├─ application/
│  ├─ app.c / app.h            # 模式状态机、1 kHz 控制、日志
│  ├─ app_config.h             # 所有可调参数
│  ├─ motor_gm6020.c / .h      # GM6020 CAN 收发与反馈解析
│  ├─ sine_profile.c / .h      # 正弦轨迹与导数
│  ├─ pid.c / .h               # 通用 PID（抗饱和、微分先行）
│  ├─ safety.c / .h            # 保护逻辑 + IWDG
│  ├─ debug_uart.c / .h        # USART1 日志与命令
│  └─ board_io.c / .h          # RGB LED、按键
├─ bsp/boards/bsp_can.c / .h   # CAN 过滤器、bus-off 恢复
├─ Src/                        # CubeMX 生成的 main / can / gpio / 中断
├─ Inc/
├─ Drivers/                    # ST HAL + CMSIS
├─ MDK-ARM/GM6020_Sine.uvprojx # Keil 工程
├─ CMakeLists.txt              # 可选 GCC 构建
├─ cmake/                      # GCC 启动文件 + 链接脚本
├─ .github/workflows/build.yml # GitHub Actions 自动编译
├─ docs/
│  ├─ wiring.md
│  ├─ build-gcc.md
│  └─ report_template.md
├─ tools/
│  ├─ plot_serial.py
│  └─ requirements.txt
└─ README.md
```

---

## 9. 常见问题

**Q1：串口一直打印等待，没有 `# RUN`？**

- 电机 24 V 是否上电；
- CAN_H/CAN_L 是否接反；
- 电机 ID 是否为 1（`0x205` 反馈）；
- CAN 是否 1 Mbps；
- 共地、终端电阻。

**Q2：电机一使能就飞车/反向？**

- 方向反了：把 `MOTOR_SIGN` 改成 `-1`；
- 也可能是 CAN 线序/电源问题，先断电机电源检查；
- 第一次测试务必空载、低幅值（先把 `SINE_SPEED_AMP_RPM` 改小）。

**Q3：位置模式来回摆但误差很大？**

- 先确认速度环跟踪正常，再调位置环；
- 检查 `POS_KP` 是否太小、`POS_MAX_RPM` 是否限得太低；
- 位置反馈是单圈 0–8191，不能做多圈绝对位置；本作业用 ±60° 就是为了避开绕圈。

**Q4：Keil 下载报错？**

- `Options → Debug` 是否选 J-Link、Port 是否 SW；
- `Utilities → Flash Download` 是否有 `STM32F4xx 1MB Flash`；
- 降低 SWD 时钟，检查 GND。

**Q5：程序运行约 0.25 s 就复位？**

- 说明 IWDG 没被喂：检查 `App_ControlStep()` 是否真的在 SysTick 里以 1 kHz 调用；
- 调试暂停时已冻结 IWDG；如果是真实运行复位，重点查中断优先级/死循环。

---

## 10. 参考与致谢

- 工程底座与 HAL 配置来自 DJI **RoboMaster/Development-Board-C-Examples** 的 `14.CAN` 例程。
- STM32F4 HAL / CMSIS 版权归 STMicroelectronics，按其 BSD-3-Clause 许可使用。
- GM6020 CAN 协议参考 RoboMaster 官方电机使用说明。

第三方许可见 [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

本项目自身代码使用 MIT License，见 [`LICENSE`](LICENSE)。
