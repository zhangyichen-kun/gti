#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ============================================================
// 收敛加密模块
// 原理：Key = SHA256(文件内容)，相同文件 → 相同密钥 → 相同密文 → 可去重
// 底层：Windows BCrypt (SHA256 + AES-256-CBC)
// ============================================================

namespace crypto {

// SHA256 哈希，返回 32 字节原始二进制
std::vector<uint8_t> sha256(const std::vector<char>& data);

// SHA256 哈希，返回 64 字符十六进制字符串
std::string sha256_hex(const std::vector<char>& data);

// AES-256-CBC 加密
// key 必须 32 字节，iv 必须 16 字节
// 返回密文（不含 IV，调用方自行管理 IV）
std::vector<char> aes_encrypt(const std::vector<char>& plaintext,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv);

// AES-256-CBC 解密
std::vector<char> aes_decrypt(const std::vector<char>& ciphertext,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv);

// 获取固定 IV（16 字节全零，保证收敛性）
std::vector<uint8_t> fixed_iv();

// 十六进制字符串转字节（解密用）
std::vector<uint8_t> hex_to_bytes(const std::string& hex);

} // namespace crypto
