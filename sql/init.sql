-- ============================================
-- CloudDisk 数据库初始化脚本
-- 在 SQL Server Management Studio 中执行
-- 或使用 sqlcmd 命令行执行
-- ============================================

-- 创建数据库（如果不存在）
IF NOT EXISTS (SELECT name FROM sys.databases WHERE name = N'CloudDisk')
BEGIN
    CREATE DATABASE CloudDisk;
    PRINT '数据库 CloudDisk 已创建';
END
ELSE
    PRINT '数据库 CloudDisk 已存在，跳过创建';
GO

-- 切换到 CloudDisk
USE CloudDisk;
GO

-- 创建文件表（如果不存在）
IF NOT EXISTS (SELECT * FROM sys.objects WHERE object_id = OBJECT_ID(N'files') AND type = 'U')
BEGIN
    CREATE TABLE files (
        id          INT IDENTITY(1,1) PRIMARY KEY,
        filename    NVARCHAR(256)   NOT NULL,    -- 原始文件名
        stored_name NVARCHAR(128)   NOT NULL,    -- 存储唯一名 (UUID)
        file_size   BIGINT          NOT NULL,    -- 文件大小(字节)
        mime_type   NVARCHAR(64)    NULL,        -- MIME 类型
        file_hash   NVARCHAR(64)    NULL,        -- SHA256 哈希（后期去重用）
        upload_time DATETIME2       DEFAULT GETDATE()
    );
    PRINT '表 files 已创建';
END
ELSE
    PRINT '表 files 已存在，跳过创建';
GO

PRINT '初始化完成！';
GO
