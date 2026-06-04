#include "util.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace util {

// ============================================================
// 生成 UUID v4
// ============================================================
std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 8; i++)  ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++)  ss << dis(gen);
    ss << "-4"; // UUID version 4
    for (int i = 0; i < 3; i++)  ss << dis(gen);
    ss << "-";
    ss << dis2(gen); // 8, 9, a, or b
    for (int i = 0; i < 3; i++)  ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);

    return ss.str();
}

// ============================================================
// 当前时间字符串
// ============================================================
std::string now_string() {
    auto now = std::time(nullptr);
    std::tm tm_buf;
    localtime_s(&tm_buf, &now); // Windows 安全版本

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// ============================================================
// 文件大小格式化
// ============================================================
std::string format_size(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }

    std::stringstream ss;
    if (unit_idx == 0) {
        ss << static_cast<int>(size) << " " << units[unit_idx];
    } else {
        ss << std::fixed << std::setprecision(1) << size << " " << units[unit_idx];
    }
    return ss.str();
}

// ============================================================
// 根据扩展名获取 MIME 类型
// ============================================================
std::string get_mime(const std::string& filename) {
    std::string ext = get_extension(filename);

    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")  return "image/png";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".bmp")  return "image/bmp";
    if (ext == ".webp") return "image/webp";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";

    return "application/octet-stream"; // 默认二进制
}

// ============================================================
// 提取文件扩展名（小写）
// ============================================================
std::string get_extension(const std::string& filename) {
    auto pos = filename.find_last_of('.');
    if (pos == std::string::npos) return "";

    std::string ext = filename.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

} // namespace util
