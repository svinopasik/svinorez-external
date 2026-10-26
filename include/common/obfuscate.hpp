#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace cs2esp {

template <std::size_t N>
struct XorStr {
    std::array<char, N> data{};
    static constexpr char key = 0x6E;

    constexpr explicit XorStr(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
            data[i] = static_cast<char>(str[i] ^ (key + static_cast<char>(i * 7 + 3)));
        }
    }

    std::string decrypt() const {
        std::string s;
        s.resize(N > 0 ? N - 1 : 0);
        for (std::size_t i = 0; i + 1 < N; ++i) {
            volatile char c = data[i];
            s[i] = static_cast<char>(c ^ (key + static_cast<char>(i * 7 + 3)));
        }
        return s;
    }
};

#define XOR_STR(str) ([]() -> std::string { \
    constexpr auto enc = ::cs2esp::XorStr<sizeof(str)>(str); \
    return enc.decrypt(); \
}())

#define XOR_CSTR(str) (XOR_STR(str).c_str())

} // namespace cs2esp
