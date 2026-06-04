#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// ODBC 头文件
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

// 文件信息结构体
struct FileInfo {
    int id;
    std::string filename;
    std::string stored_name;
    uint64_t file_size;
    std::string mime_type;
    std::string file_hash;       // SHA256 十六进制
    std::string upload_time;
};

// 数据库操作类 (SQL Server via ODBC)
class Database {
public:
    Database();
    ~Database();

    bool connect(const std::string& conn_str = "");

    // 插入文件记录（含哈希），返回新 ID
    int insert_file(const std::string& filename,
                    const std::string& stored_name,
                    uint64_t file_size,
                    const std::string& mime_type,
                    const std::string& file_hash);

    // 按哈希查找文件（秒传用），不存在返回 id=0
    FileInfo get_file_by_hash(const std::string& file_hash);

    // 获取所有文件列表
    std::vector<FileInfo> get_all_files();

    // 按 ID 获取单个文件
    FileInfo get_file_by_id(int id);

    // 获取文件哈希（解密用）
    std::string get_hash_by_id(int id);

    // 删除文件记录
    bool delete_file(int id);

    void disconnect();

private:
    SQLHENV henv_;
    SQLHDBC hdbc_;
    bool connected_;

    void check_error(SQLHANDLE handle, SQLSMALLINT type);
};
