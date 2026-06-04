#include "httplib.h"
#include "json.hpp"
#include "database.h"
#include "file_handler.h"
#include "util.h"
#include "crypto.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <windows.h>

using json = nlohmann::json;

// ============================================================
// 全局对象
// ============================================================
Database db;
FileHandler storage("storage");

// ============================================================
// 读取文件内容为字符串
// ============================================================
std::string read_file_str(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ============================================================
// 入口
// ============================================================
int main() {
    // 设置控制台 UTF-8 编码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "================================" << std::endl;
    std::cout << "  CloudDisk 云盘 v2.0 (加密)" << std::endl;
    std::cout << "================================" << std::endl;

    if (!db.connect()) {
        std::cerr << "!!! 数据库连接失败" << std::endl;
        return 1;
    }

    httplib::Server svr;

    // ====================================================
    // 静态文件
    // ====================================================
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        std::string html = read_file_str("src/static/index.html");
        if (html.empty()) {
            res.status = 404;
            res.set_content("index.html not found", "text/plain");
            return;
        }
        res.set_content(html, "text/html; charset=utf-8");
    });

    svr.Get("/style.css", [](const httplib::Request&, httplib::Response& res) {
        auto content = read_file_str("src/static/style.css");
        res.set_content(content, "text/css; charset=utf-8");
    });

    svr.Get("/app.js", [](const httplib::Request&, httplib::Response& res) {
        auto content = read_file_str("src/static/app.js");
        res.set_content(content, "text/javascript; charset=utf-8");
    });

    // ====================================================
    // API: 上传图片（收敛加密 + 去重）
    // ====================================================
    svr.Post("/api/upload", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.form.has_file("file")) {
            json j;
            j["error"] = "未选择文件";
            res.set_content(j.dump(), "application/json");
            return;
        }

        auto files = req.form.get_files("file");
        if (files.empty()) {
            json j;
            j["error"] = "文件为空";
            res.set_content(j.dump(), "application/json");
            return;
        }

        auto& file = files[0];
        std::vector<char> plaintext(file.content.begin(), file.content.end());

        // ---- 1. 计算 SHA256（收敛加密的密钥来源） ----
        std::string hash_hex = crypto::sha256_hex(plaintext);
        auto hash_raw = crypto::sha256(plaintext);  // 32 bytes, 即 AES-256 密钥

        // ---- 2. 去重检查：相同哈希 = 相同文件 ----
        FileInfo existing = db.get_file_by_hash(hash_hex);
        if (existing.id != 0) {
            // 秒传！文件已存在，只加数据库记录
            std::string ext = util::get_extension(file.filename);
            int new_id = db.insert_file(file.filename, existing.stored_name,
                                        plaintext.size(),
                                        util::get_mime(file.filename), hash_hex);
            if (new_id < 0) {
                json j;
                j["error"] = "数据库写入失败";
                res.set_content(j.dump(), "application/json");
                return;
            }

            std::cout << "[秒传] " << file.filename
                      << " -> " << existing.stored_name
                      << " (id=" << new_id << ")" << std::endl;

            json j;
            j["id"] = new_id;
            j["filename"] = file.filename;
            j["file_size"] = plaintext.size();
            j["dedup"] = true;
            j["message"] = "秒传成功（文件已存在）";
            res.set_content(j.dump(), "application/json");
            return;
        }

        // ---- 3. AES-256-CBC 加密 ----
        auto iv = crypto::fixed_iv();
        std::vector<char> ciphertext;
        try {
            ciphertext = crypto::aes_encrypt(plaintext, hash_raw, iv);
        } catch (const std::exception& e) {
            std::cerr << "[Encrypt] 加密失败: " << e.what() << std::endl;
            json j;
            j["error"] = std::string("加密失败: ") + e.what();
            res.set_content(j.dump(), "application/json");
            return;
        }

        // ---- 4. 保存密文到磁盘 ----
        std::string ext = util::get_extension(file.filename);
        std::string stored_name = util::generate_uuid() + ext;

        std::string saved = storage.save_file(ciphertext, stored_name);
        if (saved.empty()) {
            json j;
            j["error"] = "文件保存失败";
            res.set_content(j.dump(), "application/json");
            return;
        }

        // ---- 5. 写入数据库（含哈希）----
        std::string mime = util::get_mime(file.filename);
        int new_id = db.insert_file(file.filename, stored_name,
                                    ciphertext.size(), mime, hash_hex);
        if (new_id < 0) {
            storage.delete_file(stored_name);
            json j;
            j["error"] = "数据库写入失败";
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::cout << "[Upload] " << file.filename
                  << " -> " << stored_name
                  << " (id=" << new_id << ", encrypted, hash="
                  << hash_hex.substr(0, 12) << "...)" << std::endl;

        json j;
        j["id"] = new_id;
        j["filename"] = file.filename;
        j["file_size"] = ciphertext.size();
        j["dedup"] = false;
        j["message"] = "上传成功（已加密存储）";
        res.set_content(j.dump(), "application/json");
    });

    // ====================================================
    // API: 获取文件列表
    // ====================================================
    svr.Get("/api/files", [](const httplib::Request&, httplib::Response& res) {
        auto files = db.get_all_files();

        json j = json::array();
        for (auto& f : files) {
            json item;
            item["id"] = f.id;
            item["filename"] = f.filename;
            item["stored_name"] = f.stored_name;
            item["file_size"] = f.file_size;
            item["size_display"] = util::format_size(f.file_size);
            item["mime_type"] = f.mime_type;
            item["upload_time"] = f.upload_time;
            j.push_back(item);
        }

        res.set_content(j.dump(), "application/json");
    });

    // ====================================================
    // API: 下载图片（解密后返回）
    // ====================================================
    svr.Get(R"(/api/download/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int file_id = std::stoi(req.matches[1]);

        FileInfo info = db.get_file_by_id(file_id);
        if (info.id == 0) {
            res.status = 404;
            json j;
            j["error"] = "文件不存在";
            res.set_content(j.dump(), "application/json");
            return;
        }

        // 读取文件数据
        auto file_data = storage.read_file(info.stored_name);
        if (file_data.empty()) {
            res.status = 404;
            json j;
            j["error"] = "文件数据丢失";
            res.set_content(j.dump(), "application/json");
            return;
        }

        std::string content;
        // 有 hash = 加密文件，需要解密；无 hash = 旧文件，直接返回
        if (!info.file_hash.empty()) {
            try {
                auto key = crypto::hex_to_bytes(info.file_hash);
                auto iv = crypto::fixed_iv();
                auto plaintext = crypto::aes_decrypt(file_data, key, iv);
                content.assign(plaintext.data(), plaintext.size());
                std::cout << "[Download] id=" << file_id
                          << " " << info.filename << " (已解密)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[Decrypt] 解密失败: " << e.what() << std::endl;
                res.status = 500;
                json j;
                j["error"] = std::string("解密失败: ") + e.what();
                res.set_content(j.dump(), "application/json");
                return;
            }
        } else {
            content.assign(file_data.data(), file_data.size());
            std::cout << "[Download] id=" << file_id
                      << " " << info.filename << " (未加密)" << std::endl;
        }

        res.set_header("Content-Disposition",
                       "attachment; filename=\"" + info.filename + "\"");
        res.set_content(content, info.mime_type);
    });

    // ====================================================
    // API: 获取图片（页面内嵌显示）
    // ====================================================
    svr.Get(R"(/api/image/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int file_id = std::stoi(req.matches[1]);

        FileInfo info = db.get_file_by_id(file_id);
        if (info.id == 0) {
            res.status = 404;
            return;
        }

        auto file_data = storage.read_file(info.stored_name);
        if (file_data.empty()) {
            res.status = 404;
            return;
        }

        std::string content;
        if (!info.file_hash.empty()) {
            try {
                auto key = crypto::hex_to_bytes(info.file_hash);
                auto iv = crypto::fixed_iv();
                auto plaintext = crypto::aes_decrypt(file_data, key, iv);
                content.assign(plaintext.data(), plaintext.size());
            } catch (const std::exception&) {
                res.status = 500;
                return;
            }
        } else {
            content.assign(file_data.data(), file_data.size());
        }

        res.set_content(content, info.mime_type);
    });

    // ====================================================
    // API: 删除文件
    // ====================================================
    svr.Delete(R"(/api/files/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int file_id = std::stoi(req.matches[1]);

        FileInfo info = db.get_file_by_id(file_id);
        if (info.id == 0) {
            res.status = 404;
            json j;
            j["error"] = "文件不存在";
            res.set_content(j.dump(), "application/json");
            return;
        }

        storage.delete_file(info.stored_name);
        db.delete_file(file_id);

        std::cout << "[Delete] id=" << file_id
                  << " " << info.filename << std::endl;

        json j;
        j["message"] = "删除成功";
        res.set_content(j.dump(), "application/json");
    });

    // ====================================================
    // 启动
    // ====================================================
    std::string host = "0.0.0.0";
    int port = 8080;

    std::cout << std::endl;
    std::cout << "  服务已启动: http://localhost:" << port << std::endl;
    std::cout << "  按 Ctrl+C 停止服务" << std::endl;
    std::cout << "================================" << std::endl;

    svr.listen(host, port);

    return 0;
}
