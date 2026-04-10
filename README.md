# BMS 项目说明

## 项目简介

本项目是一个基于 Qt 6 的 ADS1256 采样上位机与 STM32 固件联调工程，主要用于：

- 通过串口控制 STM32 采集 ADS1256 数据
- 在桌面端实时显示单通道/多通道曲线
- 支持日志记录、CSV 数据保存与回放
- 支持基础滤波与曲线缩放分析

当前桌面端工程名为 BMS，CMake 可执行目标为 appBMS。

## 主要功能

- 串口连接与在线状态监控
- ADS1256 运行参数配置（PGA、DRATE、采样模式、通道掩码）
- 单通道采样与多通道轮询采样（SCAN8）
- 实时曲线显示、框选放大、退出放大
- 数据写入 CSV 与 SQLite
- 日志查看、日志文件/数据文件回放
- EKF 参数控制与重置

## 目录结构

- main.cpp: Qt 应用入口
- Main.qml: 主界面
- ads1256controller.cpp / ads1256controller.h: 上位机业务控制器
- logdatabase.cpp / logdatabase.h: 日志与采样数据库接口
- loglistmodel.cpp / loglistmodel.h: 日志列表模型
- samplesqlwriter.cpp / samplesqlwriter.h: 异步 SQL 写入
- crashlogger.cpp / crashlogger.h: 崩溃日志记录
- stm32.c: STM32 侧 ADS1256 采集示例与串口命令处理
- assets/icons: 应用图标资源
- scripts: 图标与打包辅助脚本
- installer/BMS.iss: Inno Setup 打包脚本

## 环境要求

- CMake >= 3.16
- Qt >= 6.10（模块：Quick、QuickDialogs2、SerialPort、Sql、Charts、Widgets）
- C++17 编译器
- Windows 下建议使用 Qt Creator + MinGW 或 MSVC 套件

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

## STM32 固件侧说明

stm32.c 提供了 ADS1256 的基础驱动流程与串口文本指令解析，支持 START/STOP、CFG、RESET、SELFCAL 等命令。

### 常用命令示例

```text
START
STOP
RESET
SELFCAL
CFG PSEL=AIN0 NSEL=AINCOM PGA=1 DRATE=0x82 MODE=RDATA ACQ=SINGLE CHMASK=0xFF
CFG ACQ=MULTI MODE=RDATA CHMASK=0x0F
```

### 输出格式示例

```text
AD:123456 HEX:0x01E240
AD8:100,200,300,400,500,600,700,800
```

## 数据与日志

- 默认数据目录在桌面 BMS_Data
- 采样时会写入 CSV
- 日志与部分采样信息会写入 SQLite（bms_logs.db）
- 上位机支持打开日志文件与数据文件进行回放

## 已做的关键时序优化（STM32 + ADS1256）

- DRDY 等待从单纯电平检测改为下降沿等待（先等待回高，再等待拉低）
- RDATA 与 RDATAC 读取后增加 DRDY 恢复延时
- 单通道 RDATA 下同通道连续采样不再每次执行 SYNC/WAKEUP
- 多通道扫描按 DRATE 增加可控建立期丢弃，减少切换污染

## 注意事项

- ADS1256 并不支持输入引脚绝对电压为负值，负值测量来自差分输入（AINP-AINN）
- 多通道轮询时需关注建立时间与有效吞吐
- 在强干扰环境建议重点检查复位引脚与地参考完整性

