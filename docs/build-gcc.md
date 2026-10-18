# 可选构建方式：CMake + arm-none-eabi-gcc

Keil 工程是作业的主要构建方式。这个 CMake 工程用于：

- 在没有 Keil 的机器上检查代码能否完整编译、链接；
- 生成 `.elf/.bin/.hex`，用 J-Link 直接下载；
- 给 GitHub Actions 做自动编译验证。

## 1. 需要安装

- CMake ≥ 3.20
- Ninja
- `arm-none-eabi-gcc`（ARM GNU Toolchain）

检查：

```powershell
cmake --version
ninja --version
arm-none-eabi-gcc --version
```

## 2. 构建

在仓库根目录执行（把工具链路径换成你的）：

```powershell
cmake -S . -B C:/temp/gm6020-build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake `
  -DARM_TOOLCHAIN_PATH="D:/path/to/arm-none-eabi/bin" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build C:/temp/gm6020-build -j 8
```

如果 `arm-none-eabi-gcc` 已经在 `PATH` 里，可以省略 `ARM_TOOLCHAIN_PATH`：

```powershell
cmake -S . -B C:/temp/gm6020-build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake `
  -DCMAKE_BUILD_TYPE=Release
```

输出：

```text
C:/temp/gm6020-build/GM6020_Sine.elf
C:/temp/gm6020-build/GM6020_Sine.bin
C:/temp/gm6020-build/GM6020_Sine.hex
C:/temp/gm6020-build/GM6020_Sine.map
```

正常会看到类似：

```text
Memory region         Used Size  Region Size  %age Used
           FLASH:       52692 B         1 MB      5.03%
             RAM:        4936 B       128 KB      3.77%
```

> 如果工程路径含中文，**build 目录请放在纯英文路径**（例如 `C:/temp/gm6020-build`），否则 Windows 下 `objcopy` 处理中文绝对路径可能报 “No such file or directory”。

## 3. 用 J-Link 下载

### 方式 A：Keil

打开 `MDK-ARM/GM6020_Sine.uvprojx`，`F8`。

### 方式 B：J-Link Commander

新建 `flash.jlink`：

```text
h
loadfile C:/temp/gm6020-build/GM6020_Sine.hex
r
g
q
```

执行：

```powershell
JLink.exe -device STM32F407IG -if SWD -speed 4000 -autoconnect 1 -CommanderScript flash.jlink
```

> 用 `.hex` 或 `.bin` 都可以；`.bin` 需要指定下载地址 `0x08000000`。

## 4. 说明

- CMake 构建和 Keil 构建使用同一套源码；
- 链接脚本在 `cmake/STM32F407IGHx_FLASH.ld`；
- 启动文件在 `cmake/startup_stm32f407xx.s`；
- 构建通过只代表代码完整、链接地址正确，**不代表硬件已经实测**。
