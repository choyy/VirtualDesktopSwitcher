#pragma once

#include <windows.h>

namespace AboutDialog {

struct Result {
    bool autoCheckUpdates = false;
    bool accepted         = false;
};

Result Show(HWND parent, bool currentAutoCheck);

} // namespace AboutDialog
