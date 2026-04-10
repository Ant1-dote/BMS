# BMS 项目说明

## 项目简介

本项目是一个基于 Qt 6 的 ADS1256 采样上位机与 STM32 固件联调工程，用于电压采样、继电器控制与充放电循环测试。

核心能力：

- 通过串口控制 STM32 采集 ADS1256 数据
- 在桌面端实时显示单通道/多通道曲线
- 通过 Modbus RTU 控制 2 路继电器（1 路充电、2 路放电）
- 一键启动自动循环充放电（与采样平级入口）
- 数据写入 CSV 与 SQLite，并支持回放分析

当前桌面端工程名为 BMS，CMake 可执行目标为 appBMS。

## 当前版本

- 版本：2.2.0

## 主要功能

- 串口连接与在线状态监控
- ADS1256 运行参数配置（PGA、DRATE、采样模式、通道掩码）
- 单通道采样与多通道轮询采样（SCAN8）
- 实时曲线显示、框选放大、退出放大
- 数据写入 CSV 与 SQLite（异步写入）
- 日志查看、日志文件/数据文件回放
- EKF 参数控制与重置
- 继电器手动控制（开关 1/2 路、全部断开）
- 自动循环充放电（先放电后充电，按阈值自动切换）

## 目录结构

- main.cpp: Qt 应用入口
- Main.qml: 主界面
- ads1256controller.cpp / ads1256controller.h: 上位机业务控制器
- logdatabase.cpp / logdatabase.h: 日志与采样数据库接口
- loglistmodel.cpp / loglistmodel.h: 日志列表模型
- samplesqlwriter.cpp / samplesqlwriter.h: 异步 SQL 写入
- crashlogger.cpp / crashlogger.h: 崩溃日志记录
- stm32.c: STM32 侧 ADS1256 + Modbus 继电器控制示例
- assets/icons: 应用图标资源
- scripts/package.ps1: 一键打包脚本
- installer/BMS.iss: Inno Setup 打包脚本

## 环境要求

- CMake >= 3.16
- Qt >= 6.10（模块：Quick、QuickDialogs2、SerialPort、Sql、Charts、Widgets）
- C++17 编译器
- Windows 下建议使用 Qt Creator + MinGW 或 MSVC 套件
- 打包安装包需安装 Inno Setup 6

## 桌面端构建与运行

### 1. 配置

```bash
cmake -S . -B build -G Ninja
```

### 2. 编译

```bash
cmake --build build
```

### 3. 运行

```bash
./build/appBMS
```

说明：在 Windows + Qt Creator 场景，通常使用 IDE 生成的构建目录（例如 Desktop_Qt_6_11_0_MinGW_64_bit-Debug）。

## UI 使用说明

### 串口与采样

在 串口连接 区域：

- 连接/断开设备
- 开始连续采样/停止连续采样
- 循环充放电（与采样平级）

点击 循环充放电 时：

- 若当前未采样，会自动启动采样
- 若当前是多通道模式，会自动切换到 SINGLE
- 自动循环将沿用当前运行配置参数

### 运行配置

运行配置区域包含：

- 采样参数（Vref/PGA/DRATE/采样模式等）
- 循环参数（放电终点、充电终点、判定点数、循环次数）

循环参数与运行配置同层管理，修改后立即作用于循环判定。

### 循环充放电逻辑

- 启动顺序：先放电，再充电
- 放电终点判定：电压 <= 放电阈值，且连续满足 N 个样本
- 充电终点判定：电压 >= 充电阈值，且连续满足 N 个样本
- 最短阶段保护：每阶段至少 3 秒，避免阈值抖动频繁切换
- 循环计数：完成 充电阶段 后计为 1 个完整循环
- 循环次数：0 表示无限循环
- 运行保护：停止采样、断开串口、切换 SCAN8 均会自动停止循环并关闭继电器

### 循环曲线显示

- 每次循环在同一图中叠加一条曲线
- 每条循环曲线的时间从 0 开始
- 不同循环使用不同颜色
- 当前默认显示最近 8 条循环曲线

## STM32 固件侧说明

stm32.c 提供 ADS1256 采样与串口命令解析，并支持通过 USART2 对继电器板发送 Modbus RTU 写单线圈命令。

### 常用命令示例

```text
START
STOP
RESET
SELFCAL
CFG PSEL=AIN0 NSEL=AINCOM PGA=1 DRATE=0x82 MODE=RDATA ACQ=SINGLE CHMASK=0xFF
CFG ACQ=SCAN8 MODE=RDATA CHMASK=0x0F
RELAY 1 ON
RELAY 1 OFF
RELAY 2 ON
RELAY 2 OFF
RELAY ALL OFF
RELAY ADDR 1
```

### 输出格式示例

```text
AD:123456 HEX:0x01E240
AD8:100,200,300,400,500,600,700,800
RELAY TX: 01 05 00 00 FF 00 8C 3A
RELAY RX: 01 05 00 00 FF 00 8C 3A
```

## 数据与日志

- 默认数据目录：桌面 BMS_Data
- 采样会写入 CSV
- 日志与采样记录写入 SQLite（bms_logs.db）
- 支持打开日志文件与数据文件回放

## 打包发布

### 一键打包（PowerShell）

```powershell
./scripts/package.ps1 -AppVersion 2.2.0
```

默认输出：

- 应用目录：dist/app
- 安装包目录：dist/installer
- 安装包文件：BMS-Setup-2.2.0.exe

## 常见问题

### 1) 循环启动后继电器不动作

- 先检查 TX/RX 是否交叉连接正确
- 确认继电器地址与 RELAY ADDR 一致
- 查看日志是否有 RELAY TX / RELAY RX / FAIL 信息

### 2) 图上没有采样曲线

- 确认设备已连接且采样已启动
- SINGLE 模式看单通道，SCAN8 模式看多通道
- 检查 DRATE、通道与缩放窗口设置

### 3) 构建后无法启动

- 确认 Qt 运行时完整部署
- 使用 scripts/package.ps1 自动执行 windeployqt

## 注意事项

- ADS1256 不支持输入引脚对地绝对负电压，负值测量来自差分输入（AINP-AINN）
- 多通道轮询需要关注建立时间与有效吞吐
- 强干扰环境建议优先检查供电、地参考与复位完整性
