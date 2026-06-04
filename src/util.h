#pragma once
#include <string>
#include <cstdint>

namespace util {

// 生成 UUID v4 格式字符串: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
std::string generate_uuid();

// 当前时间字符串: "2026-06-03 14:30:00"
std::string now_string();

// 文件大小格式化: "1.5 MB", "320 KB" 等
std::string format_size(uint64_t bytes);

// 根据扩展名返回 MIME 类型
std::string get_mime(const std::string& filename);

// 提取文件扩展名（小写）
std::string get_extension(const std::string& filename);

} // namespace util
