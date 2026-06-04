# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

CloudDisk 云盘 — 类百度网盘的图片管理系统。C++20 后端 + SQL Server + Web 前端，支持收敛加密存储和 SHA256 去重秒传。

## 构建与运行

```bash
# 构建（从项目根目录）
cmake -B build -G "MinGW Makefiles"
cmake --build build

# 仅增量重编译
cmake --build build

# 运行（需要先确保 SQL Server 已启动 + CloudDisk 数据库已建）
./build/cloud_disk.exe

# 浏览器打开
# http://localhost:8080
```

工具链：**MinGW-w64 (GCC)**，需将 `g++.exe` 所在目录加入 PATH 环境变量。

## 前置依赖

- **SQL Server** (localhost 默认实例，Windows 认证)
- 数据库初始化脚本：`sql/init.sql`
- MinGW 内置的 BCrypt (通过 `bcrypt` 链接) — 加密模块依赖，无需额外安装

## 架构

```
main.cpp  ──HTTP──►  浏览器
    │                  │
    ├── Database       │ fetch API 调用
    │   (ODBC)         │
    │   ▼              │
    │   SQL Server     ├── /        → index.html
    │                  ├── /api/upload
    ├── crypto         ├── /api/files
    │   (BCrypt)       ├── /api/image/{id}
    │                  ├── /api/download/{id}
    ├── FileHandler    └── /api/files/{id} (DELETE)
    │   (storage/)
    │
    └── util (UUID, MIME, 格式化)
```

## 模块职责

| 模块 | 文件 | 职责 |
|------|------|------|
| 入口 | `src/main.cpp` | 所有 HTTP 路由注册、静态文件 serve、全局对象初始化 |
| 数据库 | `src/database.h/.cpp` | ODBC 连接 `localhost`, `CloudDisk` 库, 文件记录 CRUD + 按哈希查询 |
| 加密 | `src/crypto.h/.cpp` | 收敛加密：SHA256(内容) → 密钥, AES-256-CBC 加解密, 固定 IV=全零, Windows BCrypt API |
| 文件 | `src/file_handler.h/.cpp` | 磁盘文件 I/O, `storage/` 目录管理 |
| 工具 | `src/util.h/.cpp` | UUID v4, MIME 推断, 文件大小格式化, 扩展名提取 |
| 前端 | `src/static/index.html`, `style.css`, `app.js` | 单页应用, 卡片网格, XMLHttpRequest 上传进度, 预览模态框 |

## 收敛加密流程

```
上传: 明文 → SHA256 → hash_hex(存库) + hash_raw(32B密钥)
        ↓ 查库去重？是 → 秒传（只插记录）
        ↓ 否
       AES-256-CBC(hash_raw, IV=全零) → 密文 → storage/UUID.png
        ↓
       插库(filename, stored_name, 密文size, mime, hash_hex)

下载: 查库得 hash_hex → hex_to_bytes → 密钥
        ↓
       读 storage/UUID.png → AES解密 → 返回明文

关键: 相同内容 → 相同SHA256 → 相同密钥 → 相同密文 → 自动去重
IV固定全零保证收敛性（对安全性有削弱但换取了去重能力）
```

## 注意事项

- **编译前确保进程已停**：旧 `cloud_disk.exe` 在运行时会占文件，导致链接失败。先 Ctrl+C 或手动结束进程。
- **终端编码**：main.cpp 开头调用了 `SetConsoleOutputCP(CP_UTF8)`，不影响代码逻辑但修复了 Windows 控制台中文乱码。
- **旧文件兼容**：下载/图片接口会检查 `file_hash` 是否为空，空则直接返回原始数据不尝试解密（兼容改版前上传的文件）。
- **BCrypt 填充标志**：加解密调用必须传 `BCRYPT_BLOCK_PADDING`，否则会在非 16 字节对齐的数据上报 `0xC0000206`。
- **数据库连接字符串**在 `src/database.cpp` 顶部 `DEFAULT_CONN_STR`，当前为 `Server=localhost` 默认实例。Express 版需改为 `Server=localhost\SQLEXPRESS`。
- **`storage/` 目录已加入 `.gitignore`**，不上传文件内容。
