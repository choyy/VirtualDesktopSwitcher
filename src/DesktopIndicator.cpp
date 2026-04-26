// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "DesktopIndicator.h"
#include "DrawingTextSTB.h"
#include "ColorParser.h"
#include "ColorState.h"
#include "ConfigIni.h"
#include <windowsx.h>
#include <vector>
#include <algorithm>

DesktopIndicator::DesktopIndicator() {
    LoadConfig();
}

DesktopIndicator::~DesktopIndicator() {
    if (m_hwnd) DestroyWindow(m_hwnd);
    CleanupFontSTB();
}

void DesktopIndicator::LoadConfig() {
    std::wstring color = ReadIniString(L"Display", L"TextColor", L"#648CFF");
    if (!color.empty()) m_textColor = color;

    int x = ReadIniInt(L"Display", L"WindowPosX", -1);
    int y = ReadIniInt(L"Display", L"WindowPosY", -1);
    if (x >= 0) m_windowPos.x = x;
    if (y >= 0) m_windowPos.y = y;

    int fs = ReadIniInt(L"Display", L"FontSize", 60);
    if (fs > 0) m_fontSize = fs;

    std::wstring curRaw = ReadIniString(L"Display", L"CurrentSymbol", EncodeSymbol(L"\u2609"));
    std::wstring cur = DecodeSymbol(curRaw);
    m_currentSymbol = (cur == L"?" ? L"\u2609" : cur);

    std::wstring othRaw = ReadIniString(L"Display", L"OtherSymbol", EncodeSymbol(L"\u25CB"));
    std::wstring oth = DecodeSymbol(othRaw);
    m_otherSymbol = (oth == L"?" ? L"\u25CB" : oth);

    std::wstring empRaw = ReadIniString(L"Display", L"EmptySymbol", EncodeSymbol(L"\u25CB"));
    std::wstring emp = DecodeSymbol(empRaw);
    m_emptySymbol = (emp == L"?" ? L"\u25CB" : emp);

    int sp = ReadIniInt(L"Display", L"CharSpacing", 0);
    if (sp >= 0) m_charSpacing = sp;

    std::wstring fn = ReadIniString(L"Display", L"FontName", L"Segoe UI Symbol");
    if (!fn.empty()) m_fontName = fn;
}

void DesktopIndicator::SaveConfig() {
    WriteIniString(L"Display", L"TextColor", m_textColor);
    WriteIniInt(L"Display", L"WindowPosX", m_windowPos.x);
    WriteIniInt(L"Display", L"WindowPosY", m_windowPos.y);
    WriteIniInt(L"Display", L"FontSize", m_fontSize);
    WriteIniString(L"Display", L"CurrentSymbol", EncodeSymbol(m_currentSymbol));
    WriteIniString(L"Display", L"OtherSymbol", EncodeSymbol(m_otherSymbol));
    WriteIniString(L"Display", L"EmptySymbol", EncodeSymbol(m_emptySymbol));
    WriteIniInt(L"Display", L"CharSpacing", m_charSpacing);
    WriteIniString(L"Display", L"FontName", m_fontName);
}

void DesktopIndicator::SetEmptySymbol(const std::wstring& sym) {
    m_emptySymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) RebuildText();
}

static int GetDpiScale() {
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return (dpi * 100 + 48) / 96;
}

bool DesktopIndicator::Initialize(HINSTANCE hInstance) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DesktopIndicatorClass";
    if (!RegisterClassW(&wc)) return false;

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"DesktopIndicatorClass", L"DesktopIndicator",
        WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, hInstance, this);

    if (!m_hwnd) return false;

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    if (!InitFontSTB(m_fontName)) {
        if (!InitFontSTB(L"Segoe UI")) {
            InitFontSTB(L"Segoe UI Symbol");
            m_fontName = L"Segoe UI Symbol";
        } else {
            m_fontName = L"Segoe UI";
        }
    }

    RebuildText();
    return true;
}

void DesktopIndicator::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    Render();
}

void DesktopIndicator::SetDesktopState(int count, int currentIndex, const std::vector<bool>& emptyDesktops) {
    m_desktopCount = count;
    m_currentDesktop = currentIndex;
    m_emptyDesktops = emptyDesktops;
    RebuildText();
}

void DesktopIndicator::RebuildText() {
    if (m_desktopCount <= 0) return;
    std::wstring text;
    for (int i = 0; i < m_desktopCount; ++i) {
        if (i == m_currentDesktop)
            text += m_currentSymbol;
        else if (i < (int)m_emptyDesktops.size() && m_emptyDesktops[i])
            text += m_emptySymbol;
        else
            text += m_otherSymbol;
    }
    m_text = text;
    Render();
}

void DesktopIndicator::SetColor(const std::wstring& hexColor) {
    m_textColor = hexColor;
    m_hasPreview = false;
    SaveConfig();
    Render();
}

void DesktopIndicator::SetColorPreview(const std::wstring& hexColor) {
    m_previewColor = hexColor;
    m_hasPreview = true;
    Render();
}

void DesktopIndicator::CancelPreview() {
    if (m_hasPreview) {
        m_hasPreview = false;
        Render();
    }
}

void DesktopIndicator::SetCurrentSymbol(const std::wstring& sym) {
    m_currentSymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) RebuildText();
}

void DesktopIndicator::SetOtherSymbol(const std::wstring& sym) {
    m_otherSymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) RebuildText();
}

void DesktopIndicator::SetFontName(const std::wstring& name) {
    m_fontName = name;
    if (InitFontSTB(m_fontName)) {
        SaveConfig();
        if (m_desktopCount > 0) RebuildText();
    }
}

void DesktopIndicator::SetCharSpacing(int spacing) {
    if (spacing < 0) spacing = 0;
    m_charSpacing = spacing;
    SaveConfig();
    if (m_desktopCount > 0) RebuildText();
}

void DesktopIndicator::ToggleEditMode() {
    SetEditMode(!m_editMode);
}

void DesktopIndicator::SetEditMode(bool edit) {
    m_editMode = edit;
    LONG exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);
    if (m_editMode) {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
        SetFocus(m_hwnd);
    } else {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
        SaveConfig();
    }
    Render();
}

void DesktopIndicator::MoveByDelta(int dx, int dy) {
    RECT r;
    GetWindowRect(m_hwnd, &r);
    m_windowPos.x = r.left + dx;
    m_windowPos.y = r.top + dy;
    SetWindowPos(m_hwnd, nullptr, m_windowPos.x, m_windowPos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void DesktopIndicator::Render() {
    if (!m_hwnd) return;

    // Build spaced text
    std::wstring spacedText;
    if (m_charSpacing > 0 && !m_text.empty()) {
        for (size_t i = 0; i < m_text.size(); ++i) {
            spacedText += m_text[i];
            if (i < m_text.size() - 1) {
                spacedText.append(m_charSpacing, L' ');
            }
        }
    } else {
        spacedText = m_text;
    }

    int textWidth = 0, textHeight = 0;
    int effectiveFontSize = m_fontSize * GetDpiScale() / 100;
    MeasureTextSTB(spacedText.c_str(), effectiveFontSize, &textWidth, &textHeight);

    int padding = 8;
    int width = (std::max)(textWidth + padding * 2, 50);
    int height = (std::max)(textHeight + padding * 2, 20);

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) { ReleaseDC(nullptr, hdcScreen); return; }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hDib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDib || !bits) {
        if (hDib) DeleteObject(hDib);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hDib);

    DWORD clearColor = m_editMode ? 0x15000000 : 0x00000000;
    int totalPixels = width * height;
    DWORD* pixels = (DWORD*)bits;
    for (int i = 0; i < totalPixels; i++) pixels[i] = clearColor;

    std::wstring colorStr = m_hasPreview ? m_previewColor : m_textColor;
    std::vector<COLORREF> renderColors;
    if (IsGradientColor(colorStr)) {
        ParseGradientColors(colorStr, renderColors);
    } else {
        renderColors.push_back(ParseColorString(colorStr));
    }
    int textAreaW = width - padding * 2;
    int textAreaH = height - padding * 2;
    RenderTextSTB(bits, width, height, padding, padding, textAreaW, textAreaH,
                  spacedText.c_str(), renderColors, effectiveFontSize);

    if (m_windowPos.x == 0 && m_windowPos.y == 0) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        m_windowPos.x = (sw - width) / 2;
        m_windowPos.y = (sh - height) / 2;
    }

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT pos = {m_windowPos.x, m_windowPos.y};
    SIZE size = {width, height};
    POINT src = {0, 0};

    UpdateLayeredWindow(m_hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOld);
    DeleteObject(hDib);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

LRESULT CALLBACK DesktopIndicator::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* overlay = (DesktopIndicator*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (overlay) return overlay->HandleMessage(msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT DesktopIndicator::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
        Render();
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        if (!m_editMode) {
            ToggleEditMode();
            SetCapture(m_hwnd);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (m_editMode) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(m_hwnd, &pt);
            RECT r;
            GetWindowRect(m_hwnd, &r);
            m_dragOffset.x = pt.x - r.left;
            m_dragOffset.y = pt.y - r.top;
            m_dragging = true;
            SetCapture(m_hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (m_dragging && (wp & MK_LBUTTON)) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(m_hwnd, &pt);
            int x = pt.x - m_dragOffset.x;
            int y = pt.y - m_dragOffset.y;
            m_windowPos.x = x;
            m_windowPos.y = y;
            SetWindowPos(m_hwnd, nullptr, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (m_dragging) {
            m_dragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (m_editMode) {
            SetEditMode(false);
            return 0;
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (m_editMode) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            int oldSize = m_fontSize;
            m_fontSize += (delta > 0) ? 4 : -4;
            m_fontSize = (std::max)(12, (std::min)(300, m_fontSize));
            if (m_fontSize != oldSize) {
                SaveConfig();
                if (m_desktopCount > 0) RebuildText();
            }
            return 0;
        }
        return 0;

    case WM_KEYDOWN:
        if (m_editMode) {
            int step = (GetKeyState(VK_CONTROL) & 0x8000) ? 10 : 1;
            switch (wp) {
            case VK_UP: MoveByDelta(0, -step); return 0;
            case VK_DOWN: MoveByDelta(0, step); return 0;
            case VK_LEFT: MoveByDelta(-step, 0); return 0;
            case VK_RIGHT: MoveByDelta(step, 0); return 0;
            }
        }
        return 0;

    case WM_OVERLAY_SETCOLOR:
        if (lp) SetColor((const wchar_t*)lp);
        return 0;

    case WM_OVERLAY_EDITMODE:
        ToggleEditMode();
        return 0;

    case WM_OVERLAY_PREVIEW_COLOR:
        if (lp) SetColorPreview((const wchar_t*)lp);
        return 0;

    case WM_OVERLAY_CANCEL_PREVIEW:
        CancelPreview();
        return 0;

    case WM_DESTROY:
        SaveConfig();
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}
