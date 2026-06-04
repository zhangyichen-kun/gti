# CloudDisk 云盘

一个类似百度网盘的图片管理系统，C++ 后端 + SQL Server + Web 前端。

## 功能

- 📤 **上传图片** — 支持多图上传，进度条显示
- 📥 **下载图片** — 一键下载原图
- 🖼️ **图片预览** — 点击缩略图放大查看
- 🗑️ **删除图片** — 支持删除操作
- 📋 **文件列表** — 网格卡片展示，缩略图 + 文件名 + 大小 + 时间

## 技术栈

| 层级 | 技术 |
|------|------|
| 后端 | C++20 + cpp-httplib |
| 数据库 | SQL Server (ODBC) |
| JSON | nlohmann/json |
| 前端 | HTML / CSS / JavaScript |
| 构建 | CMake + MinGW |

## 快速开始

### 1. 创建数据库

在 SQL Server 中执行 `sql/init.sql`，创建 CloudDisk 数据库。

### 2. 编译

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### 3. 运行

```bash
./build/cloud_disk.exe
```

### 4. 打开浏览器

访问 `http://localhost:8080`

## 项目结构

```
├── CMakeLists.txt
├── src/
│   ├── main.cpp             # 入口 + API 路由
│   ├── database.h/cpp        # SQL Server 数据库操作
│   ├── file_handler.h/cpp    # 文件存储管理
│   ├── util.h/cpp            # 工具函数
│   └── static/
│       ├── index.html        # 前端页面
│       ├── style.css         # 样式
│       └── app.js            # 交互逻辑
├── sql/
│   └── init.sql              # 数据库初始化脚本
├── third_party/
│   ├── httplib.h             # cpp-httplib
│   └── json.hpp              # nlohmann/json
└── storage/                  # 文件存储目录
```

## 后期规划

- 🔒 AES-256 加密存储
- 🔍 SHA256 文件去重
- 🧬 感知哈希模糊去重
- 👥 多用户系统
