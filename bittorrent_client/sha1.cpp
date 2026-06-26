#include "sha1.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdexcept>

static std::string hash_with_wincrypt(const uint8_t* data, size_t length) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    DWORD cbHashSize = 20;
    std::vector<uint8_t> rgbHash(20);

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("CryptAcquireContext failed");
    }

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptCreateHash failed");
    }

    if (!CryptHashData(hHash, (PBYTE)data, length, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptHashData failed");
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash.data(), &cbHashSize, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("CryptGetHashParam failed");
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return std::string((char*)rgbHash.data(), cbHashSize);
}

std::string compute_sha1(const std::string& data) {
    if (data.empty()) return hash_with_wincrypt(nullptr, 0);
    return hash_with_wincrypt((const uint8_t*)data.data(), data.size());
}

std::string compute_sha1(const std::vector<uint8_t>& data) {
    if (data.empty()) return hash_with_wincrypt(nullptr, 0);
    return hash_with_wincrypt(data.data(), data.size());
}
