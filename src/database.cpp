#include "database.h"
#include <iostream>
#include <sstream>
#include <cstring>

// ============================================================
// 默认连接字符串 - Windows 身份认证
// ============================================================
static const char* DEFAULT_CONN_STR =
    "Driver={SQL Server};"
    "Server=localhost;"
    "Database=CloudDisk;"
    "Trusted_Connection=yes;";

// ============================================================
// 辅助：从 stmt 读取 char 字段
// ============================================================
static std::string read_char(SQLHSTMT stmt, int col, char* buf, size_t buf_size) {
    memset(buf, 0, buf_size);
    SQLLEN len = 0;
    SQLGetData(stmt, col, SQL_C_CHAR, buf, buf_size, &len);
    return std::string(buf);
}

// ============================================================
// 构造 / 析构
// ============================================================
Database::Database()
    : henv_(nullptr), hdbc_(nullptr), connected_(false) {}

Database::~Database() {
    disconnect();
}

// ============================================================
// 连接数据库
// ============================================================
bool Database::connect(const std::string& conn_str) {
    const std::string& cs = conn_str.empty() ? DEFAULT_CONN_STR : conn_str;

    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv_) != SQL_SUCCESS) {
        std::cerr << "[DB] 环境句柄分配失败" << std::endl;
        return false;
    }

    SQLSetEnvAttr(henv_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    if (SQLAllocHandle(SQL_HANDLE_DBC, henv_, &hdbc_) != SQL_SUCCESS) {
        std::cerr << "[DB] 连接句柄分配失败" << std::endl;
        return false;
    }

    SQLCHAR out_str[1024];
    SQLSMALLINT out_len = 0;
    SQLRETURN ret = SQLDriverConnectA(
        hdbc_, nullptr,
        (SQLCHAR*)cs.c_str(), (SQLSMALLINT)cs.size(),
        out_str, sizeof(out_str), &out_len,
        SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "[DB] 数据库连接失败:" << std::endl;
        check_error(hdbc_, SQL_HANDLE_DBC);
        return false;
    }

    connected_ = true;
    std::cout << "[DB] SQL Server 连接成功" << std::endl;
    return true;
}

// ============================================================
// 插入文件记录（含哈希）
// ============================================================
int Database::insert_file(const std::string& filename,
                          const std::string& stored_name,
                          uint64_t file_size,
                          const std::string& mime_type,
                          const std::string& file_hash) {
    if (!connected_) return -1;

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql =
        "INSERT INTO files (filename, stored_name, file_size, mime_type, file_hash) "
        "OUTPUT INSERTED.id "
        "VALUES (?, ?, ?, ?, ?)";

    SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);

    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     256, 0, (SQLPOINTER)filename.c_str(), filename.size(), nullptr);
    SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     128, 0, (SQLPOINTER)stored_name.c_str(), stored_name.size(), nullptr);
    SQLBindParameter(stmt, 3, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                     0, 0, (SQLPOINTER)&file_size, 0, nullptr);
    SQLBindParameter(stmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, (SQLPOINTER)mime_type.c_str(), mime_type.size(), nullptr);
    SQLBindParameter(stmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, (SQLPOINTER)file_hash.c_str(), file_hash.size(), nullptr);

    SQLRETURN ret = SQLExecute(stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "[DB] 插入文件记录失败:" << std::endl;
        check_error(stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return -1;
    }

    int new_id = -1;
    SQLFetch(stmt);
    SQLGetData(stmt, 1, SQL_C_SLONG, &new_id, 0, nullptr);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return new_id;
}

// ============================================================
// 按哈希查找文件（秒传用）
// ============================================================
FileInfo Database::get_file_by_hash(const std::string& file_hash) {
    FileInfo info = {};
    if (!connected_) return info;

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql =
        "SELECT id, filename, stored_name, file_size, mime_type, "
        "file_hash, CONVERT(VARCHAR, upload_time, 120) "
        "FROM files WHERE file_hash = ?";

    SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                     64, 0, (SQLPOINTER)file_hash.c_str(), file_hash.size(), nullptr);

    SQLRETURN ret = SQLExecute(stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "[DB] 哈希查询失败:" << std::endl;
        check_error(stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return info;
    }

    if (SQLFetch(stmt) == SQL_SUCCESS) {
        char buf[512];
        info.id = 0;
        SQLGetData(stmt, 1, SQL_C_SLONG, &info.id, 0, nullptr);
        info.filename = read_char(stmt, 2, buf, sizeof(buf));
        info.stored_name = read_char(stmt, 3, buf, sizeof(buf));
        SQLGetData(stmt, 4, SQL_C_SBIGINT, &info.file_size, 0, nullptr);
        info.mime_type = read_char(stmt, 5, buf, sizeof(buf));
        info.file_hash = read_char(stmt, 6, buf, sizeof(buf));
        info.upload_time = read_char(stmt, 7, buf, sizeof(buf));
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return info;
}

// ============================================================
// 获取所有文件列表
// ============================================================
std::vector<FileInfo> Database::get_all_files() {
    std::vector<FileInfo> files;
    if (!connected_) return files;

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql = "SELECT id, filename, stored_name, file_size, "
                      "mime_type, ISNULL(file_hash, ''), "
                      "CONVERT(VARCHAR, upload_time, 120) "
                      "FROM files ORDER BY upload_time DESC";

    SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)sql, SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "[DB] 查询文件列表失败:" << std::endl;
        check_error(stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return files;
    }

    char buf[512];
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        FileInfo info;
        SQLGetData(stmt, 1, SQL_C_SLONG, &info.id, 0, nullptr);
        info.filename = read_char(stmt, 2, buf, sizeof(buf));
        info.stored_name = read_char(stmt, 3, buf, sizeof(buf));
        SQLGetData(stmt, 4, SQL_C_SBIGINT, &info.file_size, 0, nullptr);
        info.mime_type = read_char(stmt, 5, buf, sizeof(buf));
        info.file_hash = read_char(stmt, 6, buf, sizeof(buf));
        info.upload_time = read_char(stmt, 7, buf, sizeof(buf));
        files.push_back(std::move(info));
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return files;
}

// ============================================================
// 按 ID 获取文件信息
// ============================================================
FileInfo Database::get_file_by_id(int id) {
    FileInfo info = {};
    if (!connected_) return info;

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql =
        "SELECT id, filename, stored_name, file_size, mime_type, "
        "ISNULL(file_hash, ''), CONVERT(VARCHAR, upload_time, 120) "
        "FROM files WHERE id = ?";

    SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                     SQL_INTEGER, 0, 0, (SQLPOINTER)&id, 0, nullptr);

    SQLRETURN ret = SQLExecute(stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "[DB] 查询文件失败:" << std::endl;
        check_error(stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return info;
    }

    if (SQLFetch(stmt) == SQL_SUCCESS) {
        char buf[512];
        SQLGetData(stmt, 1, SQL_C_SLONG, &info.id, 0, nullptr);
        info.filename = read_char(stmt, 2, buf, sizeof(buf));
        info.stored_name = read_char(stmt, 3, buf, sizeof(buf));
        SQLGetData(stmt, 4, SQL_C_SBIGINT, &info.file_size, 0, nullptr);
        info.mime_type = read_char(stmt, 5, buf, sizeof(buf));
        info.file_hash = read_char(stmt, 6, buf, sizeof(buf));
        info.upload_time = read_char(stmt, 7, buf, sizeof(buf));
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return info;
}

// ============================================================
// 获取文件哈希（解密用）
// ============================================================
std::string Database::get_hash_by_id(int id) {
    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql = "SELECT ISNULL(file_hash, '') FROM files WHERE id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                     SQL_INTEGER, 0, 0, (SQLPOINTER)&id, 0, nullptr);

    std::string hash;
    if (SQLExecute(stmt) == SQL_SUCCESS && SQLFetch(stmt) == SQL_SUCCESS) {
        char buf[128];
        hash = read_char(stmt, 1, buf, sizeof(buf));
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return hash;
}

// ============================================================
// 删除文件记录
// ============================================================
bool Database::delete_file(int id) {
    if (!connected_) return false;

    SQLHSTMT stmt = nullptr;
    SQLAllocHandle(SQL_HANDLE_STMT, hdbc_, &stmt);

    const char* sql = "DELETE FROM files WHERE id = ?";
    SQLPrepareA(stmt, (SQLCHAR*)sql, SQL_NTS);
    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                     SQL_INTEGER, 0, 0, (SQLPOINTER)&id, 0, nullptr);

    SQLRETURN ret = SQLExecute(stmt);
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
}

// ============================================================
// 断开连接
// ============================================================
void Database::disconnect() {
    if (hdbc_) {
        SQLDisconnect(hdbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc_);
        hdbc_ = nullptr;
    }
    if (henv_) {
        SQLFreeHandle(SQL_HANDLE_ENV, henv_);
        henv_ = nullptr;
    }
    connected_ = false;
}

// ============================================================
// 错误信息输出
// ============================================================
void Database::check_error(SQLHANDLE handle, SQLSMALLINT type) {
    SQLCHAR state[6];
    SQLINTEGER native_error;
    SQLCHAR message[256];
    SQLSMALLINT message_len;
    int i = 1;

    while (SQLGetDiagRecA(type, handle, i, state,
                          &native_error, message,
                          sizeof(message), &message_len) == SQL_SUCCESS) {
        std::cerr << "  [" << state << "] " << message << std::endl;
        i++;
    }
}
