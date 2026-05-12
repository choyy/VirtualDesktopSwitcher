// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "DesktopIndicator.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>

#include "TrayIcon.h"
#include "util/DrawingTextSTB.h"
#include "util/Log.h"
#include "util/Utils.h"

namespace {

struct EnumCtx {
    std::vector<MonitorLayer> *vec;
    DesktopIndicator          *self;
    HINSTANCE                  hInst;
};

BOOL CALLBACK EnumMonitorCB(HMONITOR hMon, HDC /*unused*/, LPRECT /*unused*/, LPARAM lParam) {
    auto          *ctx = LParamToPtr<EnumCtx>(lParam);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi) == 0) { return TRUE; }
    UINT dpiX = 0, dpiY = 0;
    if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiY = 96;
    }
    bool isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    Log(L"[INFO] Monitor: " + std::wstring(&mi.szDevice[0]) + L" "
        + std::to_wstring(mi.rcMonitor.right - mi.rcMonitor.left) + L"x"
        + std::to_wstring(mi.rcMonitor.bottom - mi.rcMonitor.top) + L" dpi="
        + std::to_wstring(static_cast<int>(dpiY)) + (isPrimary ? L" PRIMARY" : L""));
    HWND hwnd = ctx->self->CreateMonitorWindow(ctx->hInst);
    if (hwnd != nullptr) {
        ctx->vec->push_back({hwnd, mi.rcMonitor, mi.rcWork, static_cast<int>(dpiY), isPrimary});
    }
    return TRUE;
}

std::wstring BuildSpacedText(const std::wstring &text, int spacing) {
    if (spacing <= 0 || text.empty()) { return text; }
    std::wstring spacedtext;
    for (size_t i = 0; i < text.size(); ++i) {
        spacedtext += text[i];
        if (i < text.size() - 1) { spacedtext.append(spacing, L' '); }
    }
    return spacedtext;
}

POINT CalcPresetPos(int preset, RECT mon, RECT work, int w, int h) {
    int monW = mon.right - mon.left;
    switch (preset) {
    case 0: return {mon.left, mon.top - 4};
    case 1: return {mon.left + (monW - w) / 2, mon.top - 4};
    case 2: return {mon.left + monW - w, mon.top - 4};
    case 3: return {mon.left, work.bottom - h + 4};
    case 4: return {mon.left + (monW - w) / 2, work.bottom - h + 4};
    case 5: return {mon.left + monW - w, work.bottom - h + 4};
    default: return {mon.left + (monW - w) / 2, mon.top - 4};
    }
}

} // namespace

DesktopIndicator::DesktopIndicator() = default;

DesktopIndicator::~DesktopIndicator() {
    for (auto &l : m_layers) {
        if (l.hwnd != nullptr) { DestroyWindow(l.hwnd); }
    }
}

HWND DesktopIndicator::CreateMonitorWindow(HINSTANCE hInst) {
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        L"DesktopIndicatorClass", L"DesktopIndicator",
        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInst, this);
    if (hwnd != nullptr) {
        SetWndUserData(hwnd, this);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return hwnd;
}

bool DesktopIndicator::Initialize(HINSTANCE hInstance) {
    if (m_pCfg == nullptr) { return false; }

    WNDCLASSW wc = {};
    if (GetClassInfoW(hInstance, L"DesktopIndicatorClass", &wc) == 0) {
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"DesktopIndicatorClass";
        if (RegisterClassW(&wc) == 0) { return false; }
    }

    m_renderer = std::make_unique<FontRenderer>();
    if (!m_renderer->Init(m_pCfg->fontName)) {
        if (!m_renderer->Init(L"Segoe UI")) {
            m_renderer->Init(L"Segoe UI Symbol");
            m_pCfg->fontName = L"Segoe UI Symbol";
        } else {
            m_pCfg->fontName = L"Segoe UI";
        }
    }

    EnumCtx ctx = {&m_layers, this, hInstance};
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCB, PtrToLParam(&ctx));

    auto prim = std::ranges::find_if(m_layers,
                                     [](auto &l) { return l.isPrimary; });
    if (prim != m_layers.end() && prim != m_layers.begin()) {
        std::swap(*prim, m_layers[0]);
    }

    Log(L"[INFO] DesktopIndicator initialized: " + std::to_wstring(m_layers.size()) + L" layers");
    RebuildText();
    return !m_layers.empty();
}

void DesktopIndicator::Show() {
    for (auto &l : m_layers) {
        ShowWindow(l.hwnd, SW_SHOW);
    }
    Render();
}

void DesktopIndicator::SetDesktopState(int count, int currentIndex,
                                       const std::array<bool, kMaxDesktops> &emptyDesktops) {
    m_desktopCount   = count;
    m_currentDesktop = currentIndex;
    m_emptyDesktops  = emptyDesktops;
    RebuildText();
}

void DesktopIndicator::RebuildText() {
    if (m_pCfg == nullptr || m_desktopCount <= 0) {
        return;
    }
    std::wstring text;
    for (int i = 0; i < m_desktopCount && i < static_cast<int>(kMaxDesktops); ++i) {
        text += (i == m_currentDesktop) ? m_pCfg->currentSymbol
                : m_emptyDesktops.at(i) ? m_pCfg->emptySymbol
                                        : m_pCfg->otherSymbol;
    }
    m_text = text;
    Render();
}

void DesktopIndicator::SetColor(const std::wstring &hexColor) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->textColor = hexColor;
    m_hasPreview      = false;
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
    if (m_pCfg == nullptr) { return; }
    m_pCfg->currentSymbol = sym;
    RebuildText();
}

void DesktopIndicator::SetOtherSymbol(const std::wstring &sym) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->otherSymbol = sym;
    RebuildText();
}

void DesktopIndicator::SetEmptySymbol(const std::wstring &sym) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->emptySymbol = sym;
    RebuildText();
}

void DesktopIndicator::SetFontName(const std::wstring &name) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->fontName = name;
    if (m_renderer && m_renderer->Init(m_pCfg->fontName)) {
        RebuildText();
    }
}

void DesktopIndicator::SetCharSpacing(int spacing) {
    if (m_pCfg == nullptr) { return; }
    spacing             = (spacing < 0) ? 0 : spacing;
    m_pCfg->charSpacing = spacing;
    RebuildText();
}

void DesktopIndicator::ToggleEditMode() {
    SetEditMode(!m_editMode);
}

void DesktopIndicator::SetEditMode(bool edit) {
    m_editMode = edit;
    if (m_pCfg != nullptr && m_editMode) { m_pCfg->positionPreset = -1; }
    for (auto &l : m_layers) {
        auto ex = static_cast<DWORD>(GetWindowLong(l.hwnd, GWL_EXSTYLE));
        if (m_editMode) {
            SetWindowLong(l.hwnd, GWL_EXSTYLE, static_cast<LONG>(ex & ~static_cast<DWORD>(WS_EX_TRANSPARENT)));
        } else {
            SetWindowLong(l.hwnd, GWL_EXSTYLE, static_cast<LONG>(ex | WS_EX_TRANSPARENT));
        }
        SetWindowPos(l.hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
    if (m_editMode && !m_layers.empty()) {
        SetCapture(m_layers[0].hwnd);
    } else {
        ReleaseCapture();
        if (m_onConfigChanged) { m_onConfigChanged(); }
    }
    Render();
}

void DesktopIndicator::SetPositionPreset(int preset) {
    if (m_pCfg == nullptr || m_renderer == nullptr) { return; }
    if (preset < 0 || preset >= kPositionPresetCount) { return; }

    auto spacedtext = BuildSpacedText(m_text, m_pCfg->charSpacing);
    int  padding    = 8;

    Log(L"[INFO] SetPositionPreset: preset=" + std::to_wstring(preset));
    m_pCfg->positionPreset = preset;
    m_pCfg->posInitialized = true;
    if (m_editMode) { SetEditMode(false); }

    for (auto &l : m_layers) {
        int fs = ScaleForDpi(m_pCfg->fontSize, l.dpi);
        int tw = 0, th = 0;
        m_renderer->Measure(spacedtext.c_str(), fs, &tw, &th);
        int w = (std::max)(tw + padding * 2, 50);
        int h = (std::max)(th + padding * 2, 20);

        POINT pos = CalcPresetPos(preset, l.monitor, l.work, w, h);
        if (&l == m_layers.data()) { m_pCfg->windowPos = pos; }
        SetWindowPos(l.hwnd, nullptr, pos.x, pos.y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void DesktopIndicator::MoveByDelta(int dx, int dy) {
    if (m_pCfg == nullptr) { return; }
    for (auto &l : m_layers) {
        RECT r;
        GetWindowRect(l.hwnd, &r);
        if (&l == m_layers.data()) {
            m_pCfg->windowPos.x = r.left + dx;
            m_pCfg->windowPos.y = r.top + dy;
        }
        SetWindowPos(l.hwnd, nullptr, r.left + dx, r.top + dy, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    m_pCfg->posInitialized = true;
    m_pCfg->positionPreset = -1;
}

void DesktopIndicator::Render() {
    if (m_layers.empty() || m_pCfg == nullptr) { return; }

    auto spacedtext = BuildSpacedText(m_text, m_pCfg->charSpacing);

    if (m_renderer == nullptr) { return; }

    std::wstring colorStr = m_hasPreview ? m_previewColor : m_pCfg->textColor;
    int          padding  = 8;

    BLENDFUNCTION blend       = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    if (!m_pCfg->posInitialized && !m_layers.empty()) {
        auto &p  = m_layers[0];
        int   fs = ScaleForDpi(m_pCfg->fontSize, p.dpi);
        int   tw = 0, th = 0;
        m_renderer->Measure(spacedtext.c_str(), fs, &tw, &th);
        int w                  = (std::max)(tw + padding * 2, 50);
        m_pCfg->windowPos.x    = p.monitor.left + (p.monitor.right - p.monitor.left - w) / 2;
        m_pCfg->windowPos.y    = p.monitor.top - 4;
        m_pCfg->posInitialized = true;
    }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = (hdcScreen != nullptr) ? CreateCompatibleDC(hdcScreen) : nullptr;

    for (auto &l : m_layers) {
        int fontSize = ScaleForDpi(m_pCfg->fontSize, l.dpi);
        int textW = 0, textH = 0;
        m_renderer->Measure(spacedtext.c_str(), fontSize, &textW, &textH);
        int w = (std::max)(textW + padding * 2, 50);
        int h = (std::max)(textH + padding * 2, 20);

        if (hdcMem == nullptr) { continue; }

        BITMAPINFO bmi              = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = w;
        bmi.bmiHeader.biHeight      = -h;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void   *bits = nullptr;
        HBITMAP hDib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if ((hDib == nullptr) || (bits == nullptr)) {
            if (hDib != nullptr) { DeleteObject(hDib); }
            continue;
        }

        auto *hOldBmp = SelectObject(hdcMem, hDib);
        DWORD clear   = m_editMode ? 0x55000000 : 0x00000000;
        std::fill_n(static_cast<DWORD *>(bits), w * h, clear);
        m_renderer->Render(bits, w, h, padding, padding, w - padding * 2, h - padding * 2,
                           spacedtext.c_str(), colorStr, fontSize);

        POINT pos  = {m_pCfg->windowPos.x + (l.monitor.left - m_layers[0].monitor.left),
                      m_pCfg->windowPos.y + (l.monitor.top - m_layers[0].monitor.top)};
        SIZE  size = {w, h};
        POINT src  = {0, 0};
        UpdateLayeredWindow(l.hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hDib);
    }
    if (hdcMem != nullptr) { DeleteDC(hdcMem); }
    if (hdcScreen != nullptr) { ReleaseDC(nullptr, hdcScreen); }
}

void DesktopIndicator::Rebuild() {
    Log(L"[INFO] Rebuild: " + std::to_wstring(m_layers.size()) + L" layers before");
    bool wasVisible = false;
    for (auto &l : m_layers) {
        if (IsWindowVisible(l.hwnd) != 0) { wasVisible = true; }
        DestroyWindow(l.hwnd);
    }
    int savedPreset = (m_pCfg != nullptr) ? m_pCfg->positionPreset : -1;
    m_layers.clear();
    m_editMode = false;
    m_dragging = false;
    ReleaseCapture();

    EnumCtx ctx = {&m_layers, this, GetModuleHandle(nullptr)};
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCB, PtrToLParam(&ctx));

    auto prim = std::ranges::find_if(m_layers,
                                     [](auto &l) { return l.isPrimary; });
    if (prim != m_layers.end() && prim != m_layers.begin()) {
        std::swap(*prim, m_layers[0]);
    }

    if (wasVisible) {
        for (auto &l : m_layers) { ShowWindow(l.hwnd, SW_SHOW); }
    }
    Render();
    Log(L"[INFO] Rebuild done: " + std::to_wstring(m_layers.size()) + L" layers");
    if (savedPreset >= 0) { SetPositionPreset(savedPreset); }
}

LRESULT CALLBACK DesktopIndicator::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto *overlay = GetWndUserData<DesktopIndicator>(hwnd);
    if (overlay != nullptr) {
        return overlay->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT DesktopIndicator::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDBLCLK:
        if (!m_editMode) { ToggleEditMode(); }
        return 0;

    case WM_LBUTTONDOWN:
        if (m_editMode) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(hwnd, &pt);
            // Convert to primary monitor coordinate system
            auto it = std::ranges::find_if(m_layers,
                                           [hwnd](const MonitorLayer &l) {
                                               return l.hwnd == hwnd;
                                           });
            if (it != m_layers.end()) {
                int winLeft    = m_pCfg->windowPos.x + (it->monitor.left - m_layers[0].monitor.left);
                int winTop     = m_pCfg->windowPos.y + (it->monitor.top - m_layers[0].monitor.top);
                m_dragOffset.x = pt.x - winLeft;
                m_dragOffset.y = pt.y - winTop;
                m_dragging     = true;
                SetCapture(hwnd);
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (m_dragging && ((wp & MK_LBUTTON) != 0u)) {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(hwnd, &pt);
            m_pCfg->windowPos.x = pt.x - m_dragOffset.x;
            m_pCfg->windowPos.y = pt.y - m_dragOffset.y;
            for (auto &l : m_layers) {
                SetWindowPos(l.hwnd, nullptr,
                             m_pCfg->windowPos.x + (l.monitor.left - m_layers[0].monitor.left),
                             m_pCfg->windowPos.y + (l.monitor.top - m_layers[0].monitor.top),
                             0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (m_dragging) {
            m_dragging = false;
            ReleaseCapture();
            if (m_pCfg != nullptr) { m_pCfg->positionPreset = -1; }
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (m_editMode) { SetEditMode(false); }
        return 0;

    case WM_MOUSEWHEEL:
        if (m_editMode && m_pCfg != nullptr) {
            int delta   = GET_WHEEL_DELTA_WPARAM(wp);
            int oldSize = m_pCfg->fontSize;
            m_pCfg->fontSize += (delta > 0) ? 4 : -4;
            m_pCfg->fontSize = (std::clamp)(m_pCfg->fontSize, 12, 300);
            if (m_pCfg->fontSize != oldSize) {
                if (m_onConfigChanged) { m_onConfigChanged(); }
                RebuildText();
            }
            return 0;
        }
        return 0;

    case WM_KEYDOWN:
        if (m_editMode) {
            int step = ((static_cast<UINT>(GetKeyState(VK_CONTROL)) & 0x8000u) != 0) ? 10 : 1;
            switch (wp) {
            case VK_UP: MoveByDelta(0, -step); return 0;
            case VK_DOWN: MoveByDelta(0, step); return 0;
            case VK_LEFT: MoveByDelta(-step, 0); return 0;
            case VK_RIGHT: MoveByDelta(step, 0); return 0;
            default: break;
            }
        }
        return 0;

    case WM_DESTROY:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
