#pragma once

#include <cstdint>

enum class LangType : std::uint8_t { Chinese,
                                     English };

namespace Lang {

void           Init();
LangType       Current();
void           Set(LangType type);
const wchar_t *Get(const wchar_t *key);

} // namespace Lang
