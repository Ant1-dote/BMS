# BMS 图标资源说明

## 文件用途
- `bms_icon_master.svg`：图标矢量母版（1024x1024）。
- `bms_app.ico`：Windows 应用图标（需要你生成并放在同目录）。
- `icons.qrc`：Qt 运行时图标资源描述文件。

## ICO 生成要求
建议在 `bms_app.ico` 内至少包含以下尺寸：
- 16x16
- 24x24
- 32x32
- 48x48
- 64x64
- 128x128
- 256x256

## 生成建议
你可以先由 `bms_icon_master.svg` 导出多尺寸 PNG，再打包为 ICO。

## 一键生成（已内置脚本）
在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\generate_icon.ps1 -ExportPng
```

生成结果：
- `assets/icons/bms_app.ico`
- `assets/icons/generated/bms_*.png`
