# 固件归档文件夹

此文件夹用于存储编译后的固件文件。

## 文件命名格式

```
firmware_[hash]_[date]_[time].bin
```

示例：`firmware_a1b2c3d4_1123_1430.bin`

- **hash**: MD5哈希值前8位
- **date**: 月日（MMDD格式）
- **time**: 时分（HHMM格式）

## 文件夹结构

```
firmware_archive/
├── 20241123/               # 日期文件夹 (YYYYMMDD)
│   ├── firmware_xxx_1123_1000.bin
│   ├── firmware_xxx_1123_1000.json  # 固件信息
│   ├── firmware_yyy_1123_1430.bin
│   └── firmware_yyy_1123_1430.json
├── 20241124/
│   └── ...
└── README.md
```

## JSON信息文件

每个固件文件都有对应的`.json`文件，包含：
- 原始路径
- 文件哈希（MD5和SHA256）
- 文件大小
- 编译时间
- 时间戳

## 注意事项

- 固件文件不会被提交到Git仓库
- 定期清理旧的固件文件以节省空间
- 重要版本可以手动备份到其他位置