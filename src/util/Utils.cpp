#include "util/Utils.h"

#include <algorithm>
#include <cmath>

void ActivateWindow(HWND hwnd) {
    HWND        foreHwnd     = GetForegroundWindow();
    DWORD       foreThreadId = 0;
    const DWORD curThreadId  = GetCurrentThreadId();
    BOOL        attached     = FALSE;

    if (foreHwnd != nullptr) {
        foreThreadId = GetWindowThreadProcessId(foreHwnd, nullptr);
        if (foreThreadId != 0 && foreThreadId != curThreadId) {
            AttachThreadInput(curThreadId, foreThreadId, TRUE);
            attached = TRUE;
        }
    }

    SetForegroundWindow(hwnd);

    if (attached != FALSE) {
        AttachThreadInput(curThreadId, foreThreadId, FALSE);
    }
}

std::wstring GetWindowTitle(HWND hwnd) {
    std::wstring buffer(256, L'\0');
    const int    length = GetWindowTextW(hwnd, buffer.data(), static_cast<int>(buffer.size()));
    if (length <= 0) {
        return {};
    }
    buffer.resize(length);
    return buffer;
}

std::wstring Utf8ToWide(const std::string &str) {
    if (str.empty()) { return {}; }
    const int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) { return {}; }
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, result.data(), len);
    result.resize(len - 1);
    return result;
}

bool IsAdminProcess() {
    HANDLE hToken{};
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) == 0) { return false; }
    TOKEN_ELEVATION elev{};
    DWORD           size = sizeof(elev);
    bool            ok   = (GetTokenInformation(hToken, TokenElevation, &elev, size, &size) != 0) && elev.TokenIsElevated != 0;
    CloseHandle(hToken);
    return ok;
}

COLORREF ParseColorString(const std::wstring &colorStr) {
    if (colorStr.empty()) { return RGB(255, 255, 255); }

    if (colorStr[0] == L'#' && colorStr.size() >= 7) {
        wchar_t *end = nullptr;
        uint32_t hex = wcstoul(colorStr.c_str() + 1, &end, 16);
        return RGB((hex >> 16u) & 0xFFu, (hex >> 8u) & 0xFFu, hex & 0xFFu);
    }

    int            r = 255, g = 255, b = 255;
    const wchar_t *s    = colorStr.c_str();
    wchar_t       *next = nullptr;
    r                   = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    g = static_cast<int>(wcstoul(s, &next, 10));
    if ((next != nullptr) && *next == L',') {
        s = next + 1;
    } else {
        return RGB(r, g, b);
    }
    b = static_cast<int>(wcstoul(s, &next, 10));
    return RGB(r, g, b);
}

size_t ParseMultiColorString(const std::wstring &colorStr, COLORREF *outColors, size_t maxColors) {
    size_t count = 0;
    size_t start = 0;
    while (start < colorStr.size() && count < maxColors) {
        size_t end = colorStr.find(L'_', start);
        if (end == std::wstring::npos) {
            outColors[count++] = ParseColorString(colorStr.substr(start));
            break;
        }
        outColors[count++] = ParseColorString(colorStr.substr(start, end - start));
        start              = end + 1;
    }
    return count;
}

int MeasureLabelWidth(HWND parent, int id) {
    HWND h = GetDlgItem(parent, id);
    if (h == nullptr) { return 0; }
    int len = GetWindowTextLengthW(h);
    if (len <= 0) { return 0; }
    std::wstring txt(len + 1, L'\0');
    GetWindowTextW(h, txt.data(), len + 1);
    HDC   hdc = GetDC(h);
    auto *hf  = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    if (hf != nullptr) { SelectObject(hdc, hf); }
    SIZE sz{};
    GetTextExtentPoint32W(hdc, txt.c_str(), len, &sz);
    ReleaseDC(h, hdc);
    return static_cast<int>(sz.cx);
}

int MeasureMaxLabelWidth(HWND parent, std::initializer_list<int> ids) {
    int maxW = 0;
    for (int id : ids) {
        maxW = std::max<int>(MeasureLabelWidth(parent, id), maxW);
    }
    return maxW;
}

COLORREF InterpolateGradientColor(const COLORREF *colors, size_t colorCount, float t) {
    if (colorCount <= 1) { return colors[0]; }
    t          = std::clamp(t, 0.0f, 1.0f);
    int   segs = static_cast<int>(colorCount) - 1;
    float segT = t * static_cast<float>(segs);
    int   idx  = static_cast<int>(segT);
    if (idx >= segs) {
        idx  = segs - 1;
        segT = 1.0f;
    } else {
        segT -= static_cast<float>(idx);
    }
    float s = 1.0f - segT;
    return RGB(
        static_cast<int>(GetRValue(colors[idx]) * s + GetRValue(colors[idx + 1]) * segT),
        static_cast<int>(GetGValue(colors[idx]) * s + GetGValue(colors[idx + 1]) * segT),
        static_cast<int>(GetBValue(colors[idx]) * s + GetBValue(colors[idx + 1]) * segT));
}

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kXn = 0.95047;
constexpr double kYn = 1.0;
constexpr double kZn = 1.08883;

double SRGBToLinear(double c) {
    if (c <= 0.04045) { return c / 12.92; }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

double LinearToSRGB(double c) {
    if (c <= 0.0031308) { return c * 12.92; }
    return 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

double LabF(double t) {
    constexpr double delta = 6.0 / 29.0;
    if (t > delta * delta * delta) { return std::cbrt(t); }
    return t / (3.0 * delta * delta) + 4.0 / 29.0;
}

double LabFInv(double t) {
    constexpr double delta = 6.0 / 29.0;
    if (t > delta) { return t * t * t; }
    return 3.0 * delta * delta * (t - 4.0 / 29.0);
}

} // namespace

void RGBToLCh(COLORREF color, double &L, double &C, double &h) {
    // sRGB → Linear
    double r = SRGBToLinear(GetRValue(color) / 255.0);
    double g = SRGBToLinear(GetGValue(color) / 255.0);
    double b = SRGBToLinear(GetBValue(color) / 255.0);

    // Linear → XYZ (D65)
    double X = 0.4124564 * r + 0.3575761 * g + 0.1804375 * b;
    double Y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b;
    double Z = 0.0193339 * r + 0.1191920 * g + 0.9503041 * b;

    // XYZ → CIE L*a*b*
    double fx = LabF(X / kXn);
    double fy = LabF(Y / kYn);
    double fz = LabF(Z / kZn);

    L            = 116.0 * fy - 16.0;
    double a     = 500.0 * (fx - fy);
    double bStar = 200.0 * (fy - fz);

    // Lab → LCh
    C = std::sqrt(a * a + bStar * bStar);
    h = std::atan2(bStar, a) * 180.0 / kPi;
    if (h < 0) { h += 360.0; }
}

COLORREF LChToRGB(double L, double C, double h) {
    // LCh → Lab
    double hRad  = h * kPi / 180.0;
    double a     = C * std::cos(hRad);
    double bStar = C * std::sin(hRad);

    // Lab → XYZ
    double fy = (L + 16.0) / 116.0;
    double fx = a / 500.0 + fy;
    double fz = fy - bStar / 200.0;

    double X = kXn * LabFInv(fx);
    double Y = kYn * LabFInv(fy);
    double Z = kZn * LabFInv(fz);

    // XYZ → Linear sRGB
    double r = 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
    double g = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
    double b = 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;

    // Clamp & Linear → sRGB
    r = std::clamp(r, 0.0, 1.0);
    g = std::clamp(g, 0.0, 1.0);
    b = std::clamp(b, 0.0, 1.0);

    return RGB(
        static_cast<int>(std::round(LinearToSRGB(r) * 255.0)),
        static_cast<int>(std::round(LinearToSRGB(g) * 255.0)),
        static_cast<int>(std::round(LinearToSRGB(b) * 255.0)));
}

void RGBToHSV(COLORREF rgb, float &h, float &s, float &v) {
    float r  = GetRValue(rgb) / 255.0f;
    float g  = GetGValue(rgb) / 255.0f;
    float b  = GetBValue(rgb) / 255.0f;
    float mx = std::max({r, g, b});
    float mn = std::min({r, g, b});
    float d  = mx - mn;
    v        = mx;
    s        = (mx > 0.001f) ? d / mx : 0.0f;
    if (d < 0.001f) {
        h = 0.0f;
        return;
    }
    if (std::fabs(mx - r) < 0.001f) {
        h = 60.0f * fmod((g - b) / d, 6.0f);
    } else if (std::fabs(mx - g) < 0.001f) {
        h = 60.0f * ((b - r) / d + 2.0f);
    } else {
        h = 60.0f * ((r - g) / d + 4.0f);
    }
    if (h < 0) { h += 360.0f; }
}

COLORREF HSVToRGB(float h, float s, float v) {
    h = fmod(h, 360.0f);
    if (h < 0) { h += 360.0f; }
    int   i = static_cast<int>(h / 60.0f) % 6;
    float f = h / 60.0f - static_cast<float>(i);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i) {
    case 0: return RGB(static_cast<int>(v * 255), static_cast<int>(t * 255), static_cast<int>(p * 255));
    case 1: return RGB(static_cast<int>(q * 255), static_cast<int>(v * 255), static_cast<int>(p * 255));
    case 2: return RGB(static_cast<int>(p * 255), static_cast<int>(v * 255), static_cast<int>(t * 255));
    case 3: return RGB(static_cast<int>(p * 255), static_cast<int>(q * 255), static_cast<int>(v * 255));
    case 4: return RGB(static_cast<int>(t * 255), static_cast<int>(p * 255), static_cast<int>(v * 255));
    default: return RGB(static_cast<int>(v * 255), static_cast<int>(p * 255), static_cast<int>(q * 255));
    }
}
