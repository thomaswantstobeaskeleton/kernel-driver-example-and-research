#pragma once
/* Custom string obfuscation - no xorstr/skCrypt (publicly spread, signatured by ACs).
 * Key from FLUSHCOMM_OBF_BASE (MAGIC-derived). Formula: c^(K+(i%17)) - differs from public libs. */

#include <string>
#include <array>
#include <cstddef>

#ifdef __has_include
#  if __has_include("flush_comm_config.h")
#    include "flush_comm_config.h"
#  endif
#endif

#ifndef FLUSHCOMM_OBF_BASE
#  define FLUSHCOMM_OBF_BASE ((unsigned char)(0x90 + ((__LINE__ + __COUNTER__) % 48)))
#endif

namespace str_obf {

template<size_t N>
struct ObfStr {
    std::array<char, N> data;
    mutable bool decrypted = false;
    static constexpr unsigned char K = (unsigned char)(FLUSHCOMM_OBF_BASE + (N & 0xF));

    constexpr ObfStr(const char (&s)[N]) : data{} {
        for (size_t i = 0; i < N; i++)
            data[i] = (char)((unsigned char)s[i] ^ (K + (unsigned char)(i % 17)));
    }

    const char* get() const {
        if (!decrypted) {
            for (size_t i = 0; i < N; i++)
                const_cast<char&>(data[i]) = (char)((unsigned char)data[i] ^ (K + (unsigned char)(i % 17)));
            const_cast<bool&>(decrypted) = true;
        }
        return data.data();
    }

    std::string str() const { return std::string(get()); }
    operator std::string() const { return str(); }
};

template<size_t N>
struct ObfStrW {
    std::array<wchar_t, N> data;
    mutable bool decrypted = false;
    static constexpr unsigned char K = (unsigned char)(FLUSHCOMM_OBF_BASE + ((N * 3) & 0xF));

    constexpr ObfStrW(const wchar_t (&s)[N]) : data{} {
        for (size_t i = 0; i < N; i++)
            data[i] = (wchar_t)((unsigned int)s[i] ^ (K + (unsigned char)(i % 19)));
    }

    const wchar_t* get() const {
        if (!decrypted) {
            for (size_t i = 0; i < N; i++)
                const_cast<wchar_t&>(data[i]) = (wchar_t)((unsigned int)data[i] ^ (K + (unsigned char)(i % 19)));
            const_cast<bool&>(decrypted) = true;
        }
        return data.data();
    }

    std::wstring wstr() const { return std::wstring(get()); }
    operator std::wstring() const { return wstr(); }
};

} // namespace str_obf

/* Replace skCrypt("x").decrypt() / _xor_("x").str() - no public xorstr/skCrypt */
#define OBF_STR(s) (str_obf::ObfStr<sizeof(s)>(s).str())
#define OBF_WSTR(s) (str_obf::ObfStrW<sizeof(s)/sizeof(wchar_t)>(s).wstr())
