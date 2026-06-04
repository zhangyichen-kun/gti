#pragma once
#include <string>
#include <vector>

// 文件存储管理类
class FileHandler {
public:
    explicit FileHandler(const std::string& storage_dir);

    // 保存文件到存储目录，返回存储后的文件名
    std::string save_file(const std::vector<char>& data,
                          const std::string& stored_name);

    // 读取文件内容
    std::vector<char> read_file(const std::string& stored_name);

    // 删除文件
    bool delete_file(const std::string& stored_name);

    // 确保存储目录存在
    void ensure_dir();

private:
    std::string storage_dir_;

    // 获取文件的完整路径
    std::string full_path(const std::string& stored_name) const;
};
