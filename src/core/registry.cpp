#include "core/registry.h"

namespace uf::reg {

namespace {

struct Key {
    HKEY h = nullptr;
    ~Key() {
        if (h) RegCloseKey(h);
    }
};

}  // namespace

std::optional<std::wstring> ReadString(HKEY root, const std::wstring& subkey, const wchar_t* value, REGSAM view) {
    Key k;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE | view, &k.h) != ERROR_SUCCESS) return std::nullopt;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(k.h, value, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) return std::nullopt;
    if (type != REG_SZ && type != REG_EXPAND_SZ) return std::nullopt;
    std::wstring data(size / sizeof(wchar_t) + 1, L'\0');
    size = static_cast<DWORD>(data.size() * sizeof(wchar_t));
    if (RegQueryValueExW(k.h, value, nullptr, &type, reinterpret_cast<BYTE*>(data.data()), &size) != ERROR_SUCCESS)
        return std::nullopt;
    data.resize(wcsnlen(data.c_str(), data.size()));
    if (type == REG_EXPAND_SZ) {
        DWORD n = ExpandEnvironmentStringsW(data.c_str(), nullptr, 0);
        std::wstring expanded(n, L'\0');
        ExpandEnvironmentStringsW(data.c_str(), expanded.data(), n);
        expanded.resize(wcsnlen(expanded.c_str(), expanded.size()));
        return expanded;
    }
    return data;
}

std::vector<std::wstring> ValueNames(HKEY root, const std::wstring& subkey, REGSAM view) {
    std::vector<std::wstring> out;
    Key k;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE | view, &k.h) != ERROR_SUCCESS) return out;
    DWORD maxLen = 0;
    RegQueryInfoKeyW(k.h, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &maxLen, nullptr, nullptr, nullptr);
    std::wstring name(maxLen + 2, L'\0');
    for (DWORD i = 0;; ++i) {
        DWORD len = static_cast<DWORD>(name.size());
        LONG r = RegEnumValueW(k.h, i, name.data(), &len, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r == ERROR_SUCCESS) out.emplace_back(name.data(), len);
    }
    return out;
}

std::vector<std::wstring> SubKeys(HKEY root, const std::wstring& subkey, REGSAM view) {
    std::vector<std::wstring> out;
    Key k;
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_ENUMERATE_SUB_KEYS | view, &k.h) != ERROR_SUCCESS) return out;
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        LONG r = RegEnumKeyExW(k.h, i, name, &len, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r == ERROR_SUCCESS) out.emplace_back(name, len);
    }
    return out;
}

LONG WriteString(HKEY root, const std::wstring& subkey, const wchar_t* value, const std::wstring& data, REGSAM view) {
    Key k;
    LONG r = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE | view, nullptr, &k.h, nullptr);
    if (r != ERROR_SUCCESS) return r;
    return RegSetValueExW(k.h, value, 0, REG_SZ, reinterpret_cast<const BYTE*>(data.c_str()),
                          static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
}

}  // namespace uf::reg
