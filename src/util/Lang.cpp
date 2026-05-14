#include "Lang.h"

#include <array>

#include "util/ConfigIni.h"

namespace {

LangType g_lang = LangType::Chinese; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

constexpr std::array kZh = {
#include "LangData_zh.inc"
};

constexpr std::array kEn = {
#include "LangData_en.inc"
};

} // namespace

void Lang::Init() {
    int override = ReadIniInt(L"General", L"Language", -1);
    if (override == 0) {
        Set(LangType::Chinese);
        return;
    }
    if (override == 1) {
        Set(LangType::English);
        return;
    }

    LANGID lid = GetUserDefaultUILanguage();
    Set(PRIMARYLANGID(lid) == LANG_ENGLISH ? LangType::English : LangType::Chinese);
}

LangType Lang::Current() {
    return g_lang;
}

void Lang::Set(LangType type) {
    g_lang = type;
}

const wchar_t *Lang::Get(const wchar_t *key) {
    const auto &table = (g_lang == LangType::English) ? kEn : kZh;
    for (const auto &[k, v] : table) {
        if (wcscmp(k, key) == 0) { return v; }
    }
    return key;
}
