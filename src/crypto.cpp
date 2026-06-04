#include "crypto.h"

#define WIN32_NO_STATUS
#include <windows.h>
#include <bcrypt.h>
#include <sstream>
#include <iomanip>

namespace crypto {

// ============================================================
// 错误检查辅助
// ============================================================
static void check_ntstatus(NTSTATUS status, const char* op) {
    if (status < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[Crypto] %s 失败: NTSTATUS=0x%08X", op, (unsigned)status);
        throw std::runtime_error(msg);
    }
}

// ============================================================
// SHA256 哈希 - 返回原始二进制 (32 bytes)
// ============================================================
std::vector<uint8_t> sha256(const std::vector<char>& data) {
    NTSTATUS status;

    // 打开 SHA256 算法提供者
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                         nullptr, 0);
    check_ntstatus(status, "BCryptOpenAlgorithmProvider(SHA256)");

    // 创建哈希句柄
    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    check_ntstatus(status, "BCryptCreateHash");

    // 喂数据
    status = BCryptHashData(hHash, (PUCHAR)data.data(),
                            (ULONG)data.size(), 0);
    check_ntstatus(status, "BCryptHashData");

    // 获取哈希长度
    DWORD hash_len = 0, result_len = 0;
    status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH,
                               (PBYTE)&hash_len, sizeof(DWORD),
                               &result_len, 0);
    check_ntstatus(status, "BCryptGetProperty(HASH_LENGTH)");

    // 完成哈希
    std::vector<uint8_t> hash(hash_len);
    status = BCryptFinishHash(hHash, hash.data(), hash_len, 0);
    check_ntstatus(status, "BCryptFinishHash");

    // 清理
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return hash;
}

// ============================================================
// SHA256 哈希 - 返回十六进制字符串 (64 chars)
// ============================================================
std::string sha256_hex(const std::vector<char>& data) {
    auto raw = sha256(data);
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto b : raw) {
        ss << std::setw(2) << (int)b;
    }
    return ss.str();
}

// ============================================================
// 固定 IV - 16 字节全零（收敛加密核心）
// ============================================================
std::vector<uint8_t> fixed_iv() {
    return std::vector<uint8_t>(16, 0);
}

// ============================================================
// AES-256-CBC 加密
// ============================================================
std::vector<char> aes_encrypt(const std::vector<char>& plaintext,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv) {
    NTSTATUS status;

    // 打开 AES 算法提供者
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM,
                                         nullptr, 0);
    check_ntstatus(status, "BCryptOpenAlgorithmProvider(AES)");

    // 设置为 CBC 模式
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PBYTE)BCRYPT_CHAIN_MODE_CBC,
                               sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    check_ntstatus(status, "BCryptSetProperty(CBC)");

    // 生成对称密钥对象
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)key.data(),
                                        (ULONG)key.size(), 0);
    check_ntstatus(status, "BCryptGenerateSymmetricKey");

    // 获取输出大小（先不加密，只查询）
    DWORD result_len = 0;
    status = BCryptEncrypt(hKey,
                           (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
                           nullptr,           // padding info
                           (PUCHAR)iv.data(), (ULONG)iv.size(),
                           nullptr, 0,        // output buffer (null for query)
                           &result_len,
                           BCRYPT_BLOCK_PADDING);
    check_ntstatus(status, "BCryptEncrypt(query)");

    // 实际加密
    std::vector<char> ciphertext(result_len);
    status = BCryptEncrypt(hKey,
                           (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
                           nullptr,
                           (PUCHAR)iv.data(), (ULONG)iv.size(),
                           (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(),
                           &result_len,
                           BCRYPT_BLOCK_PADDING);
    check_ntstatus(status, "BCryptEncrypt");

    // 清理
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return ciphertext;
}

// ============================================================
// AES-256-CBC 解密
// ============================================================
std::vector<char> aes_decrypt(const std::vector<char>& ciphertext,
                              const std::vector<uint8_t>& key,
                              const std::vector<uint8_t>& iv) {
    NTSTATUS status;

    // 打开 AES 算法提供者
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM,
                                         nullptr, 0);
    check_ntstatus(status, "BCryptOpenAlgorithmProvider(AES)");

    // 设置为 CBC 模式
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                               (PBYTE)BCRYPT_CHAIN_MODE_CBC,
                               sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    check_ntstatus(status, "BCryptSetProperty(CBC)");

    // 生成对称密钥对象
    BCRYPT_KEY_HANDLE hKey = nullptr;
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                                        (PUCHAR)key.data(),
                                        (ULONG)key.size(), 0);
    check_ntstatus(status, "BCryptGenerateSymmetricKey");

    // 获取输出大小
    DWORD result_len = 0;
    status = BCryptDecrypt(hKey,
                           (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(),
                           nullptr,
                           (PUCHAR)iv.data(), (ULONG)iv.size(),
                           nullptr, 0,
                           &result_len,
                           BCRYPT_BLOCK_PADDING);
    check_ntstatus(status, "BCryptDecrypt(query)");

    // 实际解密
    std::vector<char> plaintext(result_len);
    status = BCryptDecrypt(hKey,
                           (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(),
                           nullptr,
                           (PUCHAR)iv.data(), (ULONG)iv.size(),
                           (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
                           &result_len,
                           BCRYPT_BLOCK_PADDING);
    check_ntstatus(status, "BCryptDecrypt");

    // 清理
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return plaintext;
}

// ============================================================
// 十六进制字符串 → 字节数组
// ============================================================
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    if (hex.empty()) return bytes;
    for (size_t i = 0; i + 1 < hex.length(); i += 2) {
        int b;
        try {
            b = std::stoi(hex.substr(i, 2), nullptr, 16);
        } catch (...) {
            return {}; // 非法字符，返回空
        }
        bytes.push_back(static_cast<uint8_t>(b));
    }
    return bytes;
}

} // namespace crypto
