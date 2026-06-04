#include "file_handler.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// 构造函数
// ============================================================
FileHandler::FileHandler(const std::string& storage_dir)
    : storage_dir_(storage_dir)
{
    ensure_dir();
}

// ============================================================
// 确保存储目录存在
// ============================================================
void FileHandler::ensure_dir() {
    std::error_code ec;
    if (!fs::exists(storage_dir_, ec)) {
        fs::create_directories(storage_dir_, ec);
        std::cout << "[File] 创建存储目录: " << storage_dir_ << std::endl;
    }
}

// ============================================================
// 保存文件
// ============================================================
std::string FileHandler::save_file(const std::vector<char>& data,
                                   const std::string& stored_name) {
    std::string path = full_path(stored_name);

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[File] 无法写入文件: " << path << std::endl;
        return "";
    }

    file.write(data.data(), data.size());
    file.close();

    std::cout << "[File] 文件已保存: " << stored_name
              << " (" << data.size() << " bytes)" << std::endl;
    return stored_name;
}

// ============================================================
// 读取文件
// ============================================================
std::vector<char> FileHandler::read_file(const std::string& stored_name) {
    std::string path = full_path(stored_name);

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "[File] 无法读取文件: " << path << std::endl;
        return {};
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> data(static_cast<size_t>(size));
    file.read(data.data(), size);
    file.close();

    return data;
}

// ============================================================
// 删除文件
// ============================================================
bool FileHandler::delete_file(const std::string& stored_name) {
    std::string path = full_path(stored_name);

    std::error_code ec;
    if (fs::remove(path, ec)) {
        std::cout << "[File] 文件已删除: " << stored_name << std::endl;
        return true;
    }

    if (ec) {
        std::cerr << "[File] 删除失败: " << path << " - " << ec.message() << std::endl;
    }
    return false;
}

// ============================================================
// 获取完整路径
// ============================================================
std::string FileHandler::full_path(const std::string& stored_name) const {
    return storage_dir_ + "/" + stored_name;
}
