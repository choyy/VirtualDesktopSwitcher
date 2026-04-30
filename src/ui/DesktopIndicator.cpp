// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "DesktopIndicator.h"

#include <windowsx.h>

#include <bit>

#include "util/ConfigIni.h"
#include "util/DrawingTextSTB.h"
#include "util/Utils.h"

DesktopIndicator::DesktopIndicator() {
    LoadConfig();
}

DesktopIndicator::~DesktopIndicator() {
    if (m_hwnd != nullptr) {
        DestroyWindow(m_hwnd);
    }
}

void DesktopIndicator::LoadConfig() {
    std::wstring color = ReadIniString(L"Display", L"TextColor", L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF");
    if (!color.empty()) { m_textColor = color; }

    int x = ReadIniInt(L"Display", L"WindowPosX", -1);
    int y = ReadIniInt(L"Display", L"WindowPosY", -1);
    if (x >= 0) {
        m_windowPos.x    = x;
        m_posInitialized = true;
    }
    if (y >= 0) {
        m_windowPos.y    = y;
        m_posInitialized = true;
    }

    int fs = ReadIniInt(L"Display", L"FontSize", 20);
    if (fs > 0) { m_fontSize = fs; }

    {
        std::wstring cur = DecodeSymbol(ReadIniString(L"Display", L"CurrentSymbol", EncodeSymbol(m_currentSymbol)));
        m_currentSymbol  = (cur.size() == 1) ? cur : m_currentSymbol;
    }
    {
        std::wstring oth = DecodeSymbol(ReadIniString(L"Display", L"OtherSymbol", EncodeSymbol(m_otherSymbol)));
        m_otherSymbol    = (oth.size() == 1) ? oth : m_otherSymbol;
    }
    {
        std::wstring emp = DecodeSymbol(ReadIniString(L"Display", L"EmptySymbol", EncodeSymbol(m_emptySymbol)));
        m_emptySymbol    = (emp.size() == 1) ? emp : m_emptySymbol;
    }

    int sp = ReadIniInt(L"Display", L"CharSpacing", 0);
    if (sp >= 0) { m_charSpacing = sp; }

    std::wstring fn = ReadIniString(L"Display", L"FontName", L"Segoe UI Symbol");
    if (!fn.empty()) { m_fontName = fn; }

    m_autoCheckUpdates = ReadIniInt(L"General", L"AutoCheckUpdates", 1) != 0;
    m_positionPreset   = ReadIniInt(L"Display", L"PositionPreset", -1);
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
    WriteIniInt(L"General", L"AutoCheckUpdates", m_autoCheckUpdates ? 1 : 0);
    WriteIniInt(L"Display", L"PositionPreset", m_positionPreset);
}

void DesktopIndicator::SetEmptySymbol(const std::wstring &sym) {
    m_emptySymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) {
        RebuildText();
    }
}

void DesktopIndicator::SetAutoCheckUpdates(bool v) {
    m_autoCheckUpdates = v;
    SaveConfig();
}

bool DesktopIndicator::Initialize(HINSTANCE hInstance) {
    WNDCLASSW wc     = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DesktopIndicatorClass";
    if (RegisterClassW(&wc) == 0u) { return false; }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"DesktopIndicatorClass", L"DesktopIndicator",
        WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, hInstance, this);

    if (m_hwnd == nullptr) { return false; }

    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    m_renderer = std::make_unique<FontRenderer>();
    if (!m_renderer->Init(m_fontName)) {
        if (!m_renderer->Init(L"Segoe UI")) {
            m_renderer->Init(L"Segoe UI Symbol");
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

void DesktopIndicator::SetDesktopState(int count, int currentIndex, const std::array<bool, kMaxDesktops> &emptyDesktops) {
    m_desktopCount   = count;
    m_currentDesktop = currentIndex;
    m_emptyDesktops  = emptyDesktops;
    RebuildText();
}

void DesktopIndicator::RebuildText() {
    if (m_desktopCount <= 0) {
        return;
    }
    std::wstring text;
    for (int i = 0; i < m_desktopCount && i < static_cast<int>(kMaxDesktops); ++i) {
        if (i == m_currentDesktop) {
            text += m_currentSymbol;
        } else if (m_emptyDesktops.at(i)) {
            text += m_emptySymbol;
        } else {
            text += m_otherSymbol;
        }
    }
    m_text = text;
    Render();
}

void DesktopIndicator::SetColor(const std::wstring &hexColor) {
    m_textColor  = hexColor;
    m_hasPreview = false;
    SaveConfig();
    Render();
}

void DesktopIndicator::SetColorPreview(const std::wstring &hexColor) {
    m_previewColor = hexColor;
    m_hasPreview   = true;
    Render();
}

void DesktopIndicator::CancelPreview() {
    if (m_hasPreview) {
        m_hasPreview = false;
        Render();
    }
}

void DesktopIndicator::SetCurrentSymbol(const std::wstring &sym) {
    m_currentSymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) {
        RebuildText();
    }
}

void DesktopIndicator::SetOtherSymbol(const std::wstring &sym) {
    m_otherSymbol = sym;
    SaveConfig();
    if (m_desktopCount > 0) {
        RebuildText();
    }
}

void DesktopIndicator::SetFontName(const std::wstring &name) {
    m_fontName = name;
    if (m_renderer && m_renderer->Init(m_fontName)) {
        SaveConfig();
        if (m_desktopCount > 0) {
            RebuildText();
        }
    }
}

void DesktopIndicator::SetCharSpacing(int spacing) {
    spacing       = (spacing < 0) ? 0 : spacing;
    m_charSpacing = spacing;
    SaveConfig();
    if (m_desktopCount > 0) {
        RebuildText();
    }
}

void DesktopIndicator::ToggleEditMode() {
    SetEditMode(!m_editMode);
}

void DesktopIndicator::SetEditMode(bool edit) {
    m_editMode = edit;
    if (m_hwnd == nullptr) { return; }
    auto       exStyle     = static_cast<DWORD>(GetWindowLong(m_hwnd, GWL_EXSTYLE));
    const auto transparent = static_cast<DWORD>(WS_EX_TRANSPARENT);
    if (m_editMode) {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, static_cast<LONG>(exStyle & ~transparent));
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        SetCapture(m_hwnd);
    } else {
        SetWindowLong(m_hwnd, GWL_EXSTYLE, static_cast<LONG>(exStyle | transparent));
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ReleaseCapture();
        SaveConfig();
    }
    Render();
}

void DesktopIndicator::SetPositionPreset(int preset) {
    if (m_hwnd == nullptr) {
        return;
    }
    RECT r;
    GetWindowRect(m_hwnd, &r);
    int w  = r.right - r.left;
    int h  = r.bottom - r.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int workBottom = workArea.bottom;

    switch (preset) {
    case 0: m_windowPos = {.x = 0, .y = -4}; break;
    case 1: m_windowPos = {.x = (sw - w) / 2, .y = -4}; break;
    case 2: m_windowPos = {.x = sw - w, .y = -4}; break;
    case 3: m_windowPos = {.x = 0, .y = workBottom - h + 4}; break;
    case 4: m_windowPos = {.x = (sw - w) / 2, .y = workBottom - h + 4}; break;
    case 5: m_windowPos = {.x = sw - w, .y = workBottom - h + 4}; break;
    default: return;
    }
    m_positionPreset = preset;
    m_posInitialized = true;

    if (m_editMode) {
        SetEditMode(false);
    }

    SetWindowPos(m_hwnd, nullptr, m_windowPos.x, m_windowPos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SaveConfig();
}

void DesktopIndicator::MoveByDelta(int dx, int dy) {
    RECT r;
    GetWindowRect(m_hwnd, &r);
    m_windowPos.x    = r.left + dx;
    m_windowPos.y    = r.top + dy;
    m_posInitialized = true;
    SetWindowPos(m_hwnd, nullptr, m_windowPos.x, m_windowPos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void DesktopIndicator::Render() {
    if (m_hwnd == nullptr) {
        return;
    }

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
    if (m_renderer == nullptr) {
        return;
    }
    m_renderer->Measure(spacedText.c_str(), effectiveFontSize, &textWidth, &textHeight);

    int padding = 8;
    int width   = (textWidth + padding * 2 > 50) ? (textWidth + padding * 2) : 50;
    int height  = (textHeight + padding * 2 > 20) ? (textHeight + padding * 2) : 20;

    HDC hdcScreen = GetDC(nullptr);
    if (hdcScreen == nullptr) {
        return;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (hdcMem == nullptr) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    BITMAPINFO bmi              = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void   *bits = nullptr;
    HBITMAP hDib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if ((hDib == nullptr) || (bits == nullptr)) {
        if (hDib != nullptr) {
            DeleteObject(hDib);
        }
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    auto *hOld = static_cast<HBITMAP>(SelectObject(hdcMem, hDib));

    DWORD clearColor  = m_editMode ? 0x55000000 : 0x00000000;
    int   totalPixels = width * height;
    auto *pixels      = static_cast<DWORD *>(bits);
    std::fill_n(pixels, totalPixels, clearColor);

    std::wstring colorStr  = m_hasPreview ? m_previewColor : m_textColor;
    int          textAreaW = width - padding * 2;
    int          textAreaH = height - padding * 2;
    m_renderer->Render(bits, width, height, padding, padding, textAreaW, textAreaH,
                       spacedText.c_str(), colorStr, effectiveFontSize);

    if (!m_posInitialized) {
        int sw           = GetSystemMetrics(SM_CXSCREEN);
        m_windowPos.x    = (sw - width) / 2;
        m_windowPos.y    = 0;
        m_posInitialized = true;
    }

    BLENDFUNCTION blend       = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    POINT pos  = {.x = m_windowPos.x, .y = m_windowPos.y};
    SIZE  size = {.cx = width, .cy = height};
    POINT src  = {.x = 0, .y = 0};

    UpdateLayeredWindow(m_hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOld);
    DeleteObject(hDib);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

LRESULT CALLBACK DesktopIndicator::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *overlay = std::bit_cast<DesktopIndicator *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (overlay != nullptr) {
        return overlay->HandleMessage(msg, wp, lp);
    }
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
            m_dragging     = true;
            SetCapture(m_hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (m_dragging && ((wp & MK_LBUTTON) != 0u)) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(m_hwnd, &pt);
            int x         = pt.x - m_dragOffset.x;
            int y         = pt.y - m_dragOffset.y;
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
            int delta   = GET_WHEEL_DELTA_WPARAM(wp);
            int oldSize = m_fontSize;
            m_fontSize += (delta > 0) ? 4 : -4;
            m_fontSize = (m_fontSize < 12) ? 12 : (m_fontSize > 300) ? 300
                                                                     : m_fontSize;
            if (m_fontSize != oldSize) {
                SaveConfig();
                if (m_desktopCount > 0) { RebuildText(); }
            }
            return 0;
        }
        return 0;

    case WM_KEYDOWN:
        if (m_editMode) {
            int step = ((static_cast<unsigned int>(GetKeyState(VK_CONTROL)) & 0x8000u) != 0) ? 10 : 1;
            switch (wp) {
            case VK_UP: MoveByDelta(0, -step); return 0;
            case VK_DOWN: MoveByDelta(0, step); return 0;
            case VK_LEFT: MoveByDelta(-step, 0); return 0;
            case VK_RIGHT: MoveByDelta(step, 0); return 0;
            default: break;
            }
        }
        return 0;

    case WM_OVERLAY_SETCOLOR:
        if (lp != 0) {
            SetColor(std::bit_cast<const wchar_t *>(lp));
        }
        return 0;

    case WM_OVERLAY_EDITMODE:
        ToggleEditMode();
        return 0;

    case WM_OVERLAY_PREVIEW_COLOR:
        if (lp != 0) {
            SetColorPreview(std::bit_cast<const wchar_t *>(lp));
        }
        return 0;

    case WM_OVERLAY_CANCEL_PREVIEW:
        CancelPreview();
        return 0;

    case WM_DESTROY:
        SaveConfig();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}
