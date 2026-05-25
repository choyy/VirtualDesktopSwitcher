// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#include "DesktopIndicator.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>

#include "util/DrawingTextSTB.h"
#include "util/Log.h"
#include "util/Utils.h"

namespace {

constexpr UINT_PTR kAutoHideTimerId  = 100;
constexpr UINT_PTR kRenderTimerId    = 101;
constexpr UINT_PTR kBgSampleTimerId  = 102;
constexpr int      kRenderIntervalMs = 16;
constexpr int      kSampleIntervalMs = 2500;
constexpr int      kAutoHideShow1sMs = 1000;
constexpr int      kAutoHideShow3sMs = 3000;
// 1 秒收敛至 99%：coeff ^ (1000 / kRenderIntervalMs) == 0.01
const float kSmoothKeep  = std::pow(0.01f, kRenderIntervalMs / 1000.0f);
const float kSmoothBlend = 1.0f - kSmoothKeep;

constexpr UINT WM_INDICATOR_MOVE_WINDOW = WM_USER + 50;

const HCURSOR s_hArrow = LoadCursor(nullptr, IDC_ARROW);

constexpr int kPadding   = 8;
constexpr int kMinWidth  = 50;
constexpr int kMinHeight = 20;

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
    case 3: return {mon.left + (monW - w) / 2, (work.top + work.bottom - h) / 2};
    case 4: return {mon.left, work.bottom - h + 4};
    case 5: return {mon.left + (monW - w) / 2, work.bottom - h + 4};
    case 6: return {mon.left + monW - w, work.bottom - h + 4};
    default: return {mon.left + (monW - w) / 2, mon.top - 4};
    }
}

void ApplyContrastAdaptation(MonitorLayer &layer, size_t colorIdx, float &v, float &s) {
    double bgL = layer.bgLch_L;
    double bgC = layer.bgLch_C;

    v = (bgL > 60.0) ? std::clamp(v, 0.25f, 0.70f)
                     : std::clamp(v, 0.50f, 0.90f);

    if (s > 0.05f) {
        float targetS = 0.25f + static_cast<float>(bgC * 0.004);
        targetS       = std::clamp(targetS, 0.25f, 0.75f);
        if (s < targetS) { s += (targetS - s) * 0.5f; }
    }

    if (layer.smoothV.at(colorIdx) > 0.01f) {
        v = layer.smoothV.at(colorIdx) * kSmoothKeep + v * kSmoothBlend;
        s = layer.smoothS.at(colorIdx) * kSmoothKeep + s * kSmoothBlend;
    }
    layer.smoothV.at(colorIdx) = v;
    layer.smoothS.at(colorIdx) = s;
}

bool SampleLayerBackground(MonitorLayer &layer, HDC hdcScreen) {
    RECT wr;
    GetWindowRect(layer.hwnd, &wr);
    int wx = wr.left, wy = wr.top;
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    if (ww <= 0 || wh <= 0) { return false; }

    constexpr int expand = 8;
    int           cx = wx - expand, cy = wy - expand;
    int           cw = ww + expand * 2, ch = wh + expand * 2;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (hdcMem == nullptr) { return false; }

    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cw, ch);
    if (hBmp == nullptr) {
        DeleteDC(hdcMem);
        return false;
    }

    auto *oldBmp = SelectObject(hdcMem, hBmp);
    BOOL  ok     = BitBlt(hdcMem, 0, 0, cw, ch, hdcScreen, cx, cy, SRCCOPY);

    if (ok != 0) {
        BITMAPINFO bmi              = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = cw;
        bmi.bmiHeader.biHeight      = -ch;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<DWORD> pixels(static_cast<size_t>(cw) * ch);
        if (GetDIBits(hdcMem, hBmp, 0, ch, pixels.data(), &bmi, DIB_RGB_COLORS) != 0) {
            int totalR = 0, totalG = 0, totalB = 0, count = 0;
            int innerL = expand, innerT = expand;
            int innerR = expand + ww, innerB = expand + wh;
            for (int y = 0; y < ch; ++y) {
                for (int x = 0; x < cw; ++x) {
                    if (x >= innerL && x < innerR && y >= innerT && y < innerB) { continue; }
                    DWORD px = pixels[static_cast<size_t>(y) * cw + x];
                    totalR += GetRValue(px);
                    totalG += GetGValue(px);
                    totalB += GetBValue(px);
                    ++count;
                }
            }
            if (count > 0) {
                COLORREF avgColor = RGB(totalR / count, totalG / count, totalB / count);
                double   h{};
                RGBToLCh(avgColor, layer.bgLch_L, layer.bgLch_C, h);
                layer.bgSampleValid = true;
            }
        }
    }

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    return true;
}

void ComputeScales(MonitorLayer &layer, float avgSymW, bool isDragging) {
    constexpr float maxExtra = 1.0f;
    POINT           cursorPt{};
    RECT            layerRect{};
    if (isDragging) {
        GetCursorPos(&cursorPt);
        GetWindowRect(layer.hwnd, &layerRect);
    }

    for (int i = 0; i < static_cast<int>(layer.symbolScales.size()); ++i) {
        float targetScale = 1.0f;
        if (isDragging) {
            float symCenterX = static_cast<float>(kPadding) + avgSymW * (static_cast<float>(i) + 0.5f);
            float symCenterY = static_cast<float>(layerRect.bottom - layerRect.top) * 0.5f;
            float sigmaX     = std::max(avgSymW * 4.0f, 300.0f);
            float sigmaY     = sigmaX / 2.0f;
            float dx         = static_cast<float>(cursorPt.x - layerRect.left) - symCenterX;
            float dy         = static_cast<float>(cursorPt.y - layerRect.top) - symCenterY;
            float dist2      = (dx * dx) / (sigmaX * sigmaX) + (dy * dy) / (sigmaY * sigmaY);
            targetScale      = 1.0f + maxExtra * std::exp(-0.5f * dist2);
        }
        float &cur  = layer.symbolScales.at(i);
        float  rate = (targetScale > cur) ? 0.4f : 0.15f;
        cur += (targetScale - cur) * rate;
    }
}

SymbolMetrics CalcSymbolMetrics(FontRenderer &renderer, const IndicatorConfig &cfg,
                                const std::wstring &text, const MonitorLayer &layer) {
    SymbolMetrics s;
    s.fontSize = MulDiv(cfg.fontSize, layer.dpi, 96);
    s.spacing  = cfg.charSpacing * renderer.Measure(L" ", s.fontSize).cx;

    int symCount = static_cast<int>(text.size());
    int totalW = 0, maxH = 0;
    for (int i = 0; i < symCount; ++i) {
        int  symFont   = static_cast<int>(static_cast<float>(s.fontSize) * layer.symbolScales.at(i));
        auto size      = renderer.Measure(std::wstring(1, text[i]).c_str(), symFont);
        s.widths.at(i) = size.cx;
        totalW += size.cx;
        maxH = std::max(static_cast<int>(size.cy), maxH);
    }
    if (symCount > 1) { totalW += s.spacing * (symCount - 1); }
    s.dibSize = {std::max(totalW + kPadding * 2, kMinWidth), std::max(maxH + kPadding * 2, kMinHeight)};
    return s;
}

} // namespace

HHOOK             DesktopIndicator::s_dragHook = nullptr;
DesktopIndicator *DesktopIndicator::s_instance = nullptr;

void DesktopIndicator::EnumerateMonitors(HINSTANCE hInstance) {
    EnumCtx ctx = {&m_layers, this, hInstance};
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCB, PtrToLParam(&ctx));
    auto prim = std::ranges::find_if(m_layers,
                                     [](auto &l) { return l.isPrimary; });
    if (prim != m_layers.end() && prim != m_layers.begin()) {
        std::swap(*prim, m_layers[0]);
    }
}

DesktopIndicator::DesktopIndicator() {
    s_instance = this;
}

DesktopIndicator::~DesktopIndicator() {
    UninstallDragHook();
    if (s_instance == this) { s_instance = nullptr; }
    for (auto &l : m_layers) {
        if (l.hwnd != nullptr) { DestroyWindow(l.hwnd); }
    }
}

void DesktopIndicator::RegisterMouseWheelInput() {
    if (m_layers.empty()) { return; }
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage    = 0x01;
    rid.usUsage        = 0x02;
    rid.dwFlags        = RIDEV_INPUTSINK;
    rid.hwndTarget     = m_layers[0].hwnd;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == 0) {
        Log(L"[ERROR] RegisterRawInputDevices failed");
    }
}

void DesktopIndicator::InstallDragHook() {
    if (s_dragHook == nullptr) {
        s_dragHook = SetWindowsHookExW(WH_MOUSE_LL, DragHookProc,
                                       GetModuleHandleW(nullptr), 0);
        if (s_dragHook != nullptr) {
            Log(L"[INFO] Drag hook installed");
        } else {
            Log(L"[ERROR] Drag hook install failed");
        }
    }
}

void DesktopIndicator::UninstallDragHook() {
    if (s_dragHook != nullptr) {
        UnhookWindowsHookEx(s_dragHook);
        s_dragHook = nullptr;
        Log(L"[INFO] Drag hook uninstalled");
    }
}

bool DesktopIndicator::GetSymbolIndexAt(POINT screenPt, int &outIndex) const {
    for (const auto &l : m_layers) {
        RECT wr;
        GetWindowRect(l.hwnd, &wr);
        if (PtInRect(&wr, screenPt) == 0) { continue; }

        auto clientX = static_cast<float>(screenPt.x - wr.left);
        for (int i = 0; i < static_cast<int>(m_text.size()); ++i) {
            if (l.symbolHalfWidths.at(i) <= 0.0f) { continue; }
            if (std::fabs(clientX - l.symbolCenters.at(i)) <= l.symbolHalfWidths.at(i) + 3.0f) {
                outIndex = i;
                return true;
            }
        }
    }
    return false;
}

LRESULT CALLBACK DesktopIndicator::DragHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || s_instance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto *hs = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam); // NOLINT

    if (wParam == WM_LBUTTONDOWN) {
        bool onSelf = false;
        for (const auto &l : s_instance->m_layers) {
            RECT wr;
            GetWindowRect(l.hwnd, &wr);
            if (PtInRect(&wr, hs->pt) != 0) {
                onSelf = true;
                break;
            }
        }
        if (!onSelf) {
            s_instance->m_pendingDrag     = true;
            s_instance->m_lastHoverSymbol = -1;
        }
    }

    if (wParam == WM_MOUSEMOVE && (s_instance->m_pendingDrag || s_instance->m_draggingWindow)) {
        GUITHREADINFO info       = {.cbSize = sizeof(info)};
        bool          inMoveSize = GetGUIThreadInfo(0, &info) != 0 && (info.flags & GUI_INMOVESIZE) != 0;

        if (!s_instance->m_draggingWindow) {
            if (inMoveSize) {
                CURSORINFO ci = {.cbSize = sizeof(ci)};
                if (GetCursorInfo(&ci) != 0 && ci.hCursor == s_hArrow) {
                    s_instance->m_pendingDrag    = false;
                    s_instance->m_dragHwnd       = info.hwndMoveSize;
                    s_instance->m_draggingWindow = true;
                    s_instance->UpdateRenderTimer();
                }
            }
        } else {
            if (!inMoveSize) {
                s_instance->ResetDragState();
            } else {
                int symIdx = -1;
                if (s_instance->GetSymbolIndexAt(hs->pt, symIdx)) {
                    if (symIdx != s_instance->m_lastHoverSymbol) {
                        s_instance->m_lastHoverSymbol = symIdx;
                        PostMessageW(s_instance->m_layers[0].hwnd, WM_INDICATOR_MOVE_WINDOW,
                                     static_cast<WPARAM>(symIdx),
                                     reinterpret_cast<LPARAM>(s_instance->m_dragHwnd)); // NOLINT
                    }
                } else {
                    s_instance->m_lastHoverSymbol = -1;
                }
            }
        }
    }

    if (wParam == WM_LBUTTONUP) {
        s_instance->ResetDragState();
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
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

    // 1. Register window class
    WNDCLASSW wc = {};
    if (GetClassInfoW(hInstance, L"DesktopIndicatorClass", &wc) == 0) {
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = hInstance;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"DesktopIndicatorClass";
        if (RegisterClassW(&wc) == 0) { return false; }
    }

    // 2. Initialize font renderer
    m_renderer = std::make_unique<FontRenderer>();
    if (!m_renderer->Init(m_pCfg->fontName)) {
        m_pCfg->fontName = L"Segoe UI Symbol";
        m_renderer->Init(m_pCfg->fontName);
    }

    // 3. Build text and enumerate monitors
    RebuildText();
    EnumerateMonitors(hInstance);
    Log(L"[INFO] DesktopIndicator initialized: " + std::to_wstring(m_layers.size()) + L" layers");

    // 4. Restore or calculate default window positions
    if (m_pCfg->posInitialized && !m_layers.empty()) {
        if (m_pCfg->positionPreset >= 0) {
            SetPositionPreset(m_pCfg->positionPreset);
        } else {
            for (auto &l : m_layers) {
                POINT pos = {m_pCfg->windowPos.x + (l.monitor.left - m_layers[0].monitor.left),
                             m_pCfg->windowPos.y + (l.monitor.top - m_layers[0].monitor.top)};
                SetWindowPos(l.hwnd, nullptr, pos.x, pos.y, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
    if (!m_pCfg->posInitialized && !m_layers.empty()) {
        int  fs                = MulDiv(m_pCfg->fontSize, m_layers[0].dpi, 96);
        auto spacedtext        = BuildSpacedText(m_text, m_pCfg->charSpacing);
        auto size              = m_renderer->Measure(spacedtext.c_str(), fs);
        int  w                 = std::max(static_cast<int>(size.cx) + kPadding * 2, kMinWidth);
        m_pCfg->windowPos.x    = m_layers[0].monitor.left + (m_layers[0].monitor.right - m_layers[0].monitor.left - w) / 2;
        m_pCfg->windowPos.y    = m_layers[0].monitor.top - 4;
        m_pCfg->posInitialized = true;
    }

    // 5. Build initial text and apply display mode
    ApplyShowMode(m_pCfg->showMode);

    // 6. Register Raw Input for mouse wheel
    RegisterMouseWheelInput();
    InstallDragHook();

    return !m_layers.empty();
}

void DesktopIndicator::SetDesktopState(int count, int currentIndex,
                                       const std::array<bool, kMaxDesktops> &emptyDesktops) {
    m_desktopCount   = count;
    m_currentDesktop = currentIndex;
    m_emptyDesktops  = emptyDesktops;
    RebuildText();
}

void DesktopIndicator::ShowTemporarily() {
    if (m_pCfg == nullptr || m_pCfg->showMode < ShowMode::Show1s || m_layers.empty()) { return; }
    for (auto &l : m_layers) { ShowWindow(l.hwnd, SW_SHOW); }
    KillTimer(m_layers[0].hwnd, kAutoHideTimerId);
    int ms = (m_pCfg->showMode == ShowMode::Show1s) ? kAutoHideShow1sMs : kAutoHideShow3sMs;
    SetTimer(m_layers[0].hwnd, kAutoHideTimerId, ms, AutoHideTimer);
}

void DesktopIndicator::ApplyShowMode(ShowMode mode) {
    if (m_pCfg == nullptr) { return; }
    if (!m_layers.empty()) { KillTimer(m_layers[0].hwnd, kAutoHideTimerId); }
    if (mode == ShowMode::AlwaysHide) {
        for (auto &l : m_layers) { ShowWindow(l.hwnd, SW_HIDE); }
        StopBgSampleTimer();
        UpdateRenderTimer();
    } else {
        for (auto &l : m_layers) { ShowWindow(l.hwnd, SW_SHOW); }
        if (mode >= ShowMode::Show1s) { ShowTemporarily(); }
        StartBgSampleTimer();
    }
}

void DesktopIndicator::SetShowMode(ShowMode mode) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->showMode = mode;
    ApplyShowMode(mode);
}

void CALLBACK DesktopIndicator::AutoHideTimer(HWND hwnd, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
    KillTimer(hwnd, kAutoHideTimerId);
    auto *overlay = GetWndUserData<DesktopIndicator>(hwnd);
    if (overlay != nullptr) {
        for (auto &l : overlay->m_layers) { ShowWindow(l.hwnd, SW_HIDE); }
    }
}

void DesktopIndicator::SetAnimMode(bool on) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->animMode = on ? 1 : 0;
    UpdateRenderTimer();
    Render();
}

void DesktopIndicator::SetAutoContrast(bool on) {
    if (m_pCfg == nullptr) { return; }
    m_pCfg->autoContrast = on;
    if (!m_layers.empty()) {
        if (on && m_pCfg->showMode != ShowMode::AlwaysHide) {
            StartBgSampleTimer();
            SampleBackground();
        } else {
            StopBgSampleTimer();
        }
    }
    UpdateRenderTimer();
    Render();
}

void CALLBACK DesktopIndicator::RenderTimer(HWND hwnd, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
    auto *overlay = GetWndUserData<DesktopIndicator>(hwnd);
    if (overlay == nullptr || overlay->m_pCfg == nullptr) { return; }
    if (overlay->m_pCfg->showMode == ShowMode::AlwaysHide) { return; }
    if (overlay->m_pCfg->showMode >= ShowMode::Show1s && IsWindowVisible(hwnd) == 0) { return; }
    overlay->Render();
}

void CALLBACK DesktopIndicator::BgSampleTimer(HWND hwnd, UINT /*uMsg*/, UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
    auto *overlay = GetWndUserData<DesktopIndicator>(hwnd);
    if (overlay == nullptr || overlay->m_pCfg == nullptr || !overlay->m_pCfg->autoContrast) { return; }
    if (overlay->m_pCfg->showMode == ShowMode::AlwaysHide) { return; }
    if (overlay->m_pCfg->showMode >= ShowMode::Show1s && IsWindowVisible(hwnd) == 0) { return; }
    overlay->SampleBackground();
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
    if (m_pCfg->positionPreset >= 0) { ApplyPresetPosition(); }
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
        m_pCfg->SaveToIni();
    }
    Render();
}

void DesktopIndicator::SetPositionPreset(int preset) {
    if (m_pCfg == nullptr || m_renderer == nullptr) { return; }
    if (preset < 0 || preset >= static_cast<int>(PositionPreset::Count)) { return; }

    Log(L"[INFO] SetPositionPreset: preset=" + std::to_wstring(preset));
    m_pCfg->positionPreset = preset;
    m_pCfg->posInitialized = true;
    if (m_editMode) { SetEditMode(false); }

    ApplyPresetPosition();
}

void DesktopIndicator::ApplyPresetPosition() {
    if (m_pCfg == nullptr || m_renderer == nullptr) { return; }
    int preset = m_pCfg->positionPreset;
    if (preset < 0 || preset >= static_cast<int>(PositionPreset::Count)) { return; }

    auto spacedtext = BuildSpacedText(m_text, m_pCfg->charSpacing);

    for (auto &l : m_layers) {
        int  fs   = MulDiv(m_pCfg->fontSize, l.dpi, 96);
        auto size = m_renderer->Measure(spacedtext.c_str(), fs);
        int  w    = std::max(static_cast<int>(size.cx) + kPadding * 2, kMinWidth);
        int  h    = std::max(static_cast<int>(size.cy) + kPadding * 2, kMinHeight);

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

void DesktopIndicator::SampleBackground() {
    HDC hdcScreen = GetDC(nullptr);
    if (hdcScreen == nullptr) { return; }

    for (auto &l : m_layers) {
        SampleLayerBackground(l, hdcScreen);
    }
    ReleaseDC(nullptr, hdcScreen);
}

void DesktopIndicator::UpdateRenderTimer() {
    if (m_layers.empty()) { return; }
    bool needsTimer = (m_pCfg != nullptr) && m_pCfg->showMode != ShowMode::AlwaysHide
                      && (m_pCfg->animMode != 0 || m_pCfg->autoContrast || m_draggingWindow || NeedsSettleRender());
    if (needsTimer) {
        SetTimer(m_layers[0].hwnd, kRenderTimerId, kRenderIntervalMs, RenderTimer);
    } else {
        KillTimer(m_layers[0].hwnd, kRenderTimerId);
    }
}

void DesktopIndicator::ResetDragState() {
    m_pendingDrag     = false;
    m_dragHwnd        = nullptr;
    m_draggingWindow  = false;
    m_lastHoverSymbol = -1;
    UpdateRenderTimer();
}

bool DesktopIndicator::NeedsSettleRender() const {
    for (const auto &l : m_layers) {
        for (int i = 0; i < static_cast<int>(m_text.size()); ++i) {
            if (l.symbolScales.at(i) > 1.005f) {
                return true;
            }
        }
    }
    return false;
}

void DesktopIndicator::StartBgSampleTimer() {
    if ((m_pCfg != nullptr) && m_pCfg->autoContrast
        && m_pCfg->showMode != ShowMode::AlwaysHide
        && !m_layers.empty()) {
        SetTimer(m_layers[0].hwnd, kBgSampleTimerId, kSampleIntervalMs, BgSampleTimer);
    }
}

void DesktopIndicator::StopBgSampleTimer() {
    if (!m_layers.empty()) {
        KillTimer(m_layers[0].hwnd, kBgSampleTimerId);
    }
    for (auto &l : m_layers) { l.bgSampleValid = false; }
}

std::wstring DesktopIndicator::BuildLayerColors(MonitorLayer                  &layer,
                                                const std::array<COLORREF, 5> &baseColors, size_t colorCount) const {
    if (m_hasPreview) { return m_previewColor; }
    if (colorCount == 0) { return m_pCfg->textColor; }

    float hueOff = 0.0f;
    if (m_pCfg->animMode != 0) {
        ULONGLONG ms = GetTickCount64();
        hueOff       = static_cast<float>(fmod(static_cast<double>(ms) / 12000.0 * 360.0, 360.0));
    }

    std::wstring colorStr;
    for (size_t i = 0; i < colorCount; ++i) {
        float h{}, s{}, v{};
        RGBToHSV(baseColors.at(i), h, s, v);

        if (m_pCfg->animMode != 0) {
            h = fmod(h + hueOff, 360.0f);
        }

        if (m_pCfg->autoContrast && layer.bgSampleValid) {
            ApplyContrastAdaptation(layer, i, v, s);
        }

        COLORREF               c = HSVToRGB(h, s, v);
        std::array<wchar_t, 8> buf{};
        swprintf_s(buf.data(), buf.size(), L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c)); // NOLINT
        if (i > 0) { colorStr += L'_'; }
        colorStr += buf.data();
    }
    return colorStr;
}

void DesktopIndicator::PresentLayer(MonitorLayer        &layer,
                                    const SymbolMetrics &metrics,
                                    int centerW, const ColorArray &colors,
                                    HDC hdcScreen, HDC hdcMem) {
    int w = metrics.dibSize.cx;
    int h = metrics.dibSize.cy;

    BITMAPINFO bmi              = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void   *bits = nullptr;
    HBITMAP hDib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hDib == nullptr || bits == nullptr) {
        if (hDib != nullptr) { DeleteObject(hDib); }
        return;
    }

    auto *hOldBmp = SelectObject(hdcMem, hDib);
    DWORD clear   = m_editMode ? 0x55000000 : 0x00000000;
    std::fill_n(static_cast<DWORD *>(bits), w * h, clear);

    int curX = kPadding;
    for (int i = 0; i < static_cast<int>(m_text.size()); ++i) {
        int                    symFont  = static_cast<int>(static_cast<float>(metrics.fontSize) * layer.symbolScales.at(i));
        COLORREF               symColor = (colors.count >= 2)
                                              ? InterpolateGradientColor(colors.colors.data(), colors.count,
                                                                         static_cast<float>(i) / static_cast<float>(std::max(static_cast<int>(m_text.size()) - 1, 1)))
                                              : colors.colors[0];
        std::array<wchar_t, 8> colorBuf{};
        swprintf_s(colorBuf.data(), colorBuf.size(), L"#%02X%02X%02X", // NOLINT
                   GetRValue(symColor), GetGValue(symColor), GetBValue(symColor));
        std::wstring symColorStr(colorBuf.data());
        std::wstring sym(1, m_text[i]);
        m_renderer->Render(bits, w, h, curX, kPadding, metrics.widths.at(i), h - kPadding * 2,
                           sym.c_str(), symColorStr, symFont);
        layer.symbolCenters.at(i)    = static_cast<float>(curX) + static_cast<float>(metrics.widths.at(i)) * 0.5f;
        layer.symbolHalfWidths.at(i) = static_cast<float>(metrics.widths.at(i)) * 0.5f;
        curX += metrics.widths.at(i) + metrics.spacing;
    }

    POINT pos  = {0, 0};
    SIZE  size = {w, h};
    POINT src  = {0, 0};
    if (layer.monitor.left != layer.monitor.right) {
        int anchorX = m_pCfg->windowPos.x + (layer.monitor.left - m_layers[0].monitor.left);
        int anchorY = m_pCfg->windowPos.y + (layer.monitor.top - m_layers[0].monitor.top);
        if (w != centerW) {
            int cx = anchorX - (w - centerW) / 2;
            cx     = std::clamp(cx, static_cast<int>(layer.monitor.left), static_cast<int>(layer.monitor.right - w));
            pos    = {cx, anchorY};
        } else {
            pos = {anchorX, anchorY};
        }
    }
    BLENDFUNCTION blend = {.BlendOp = AC_SRC_OVER, .SourceConstantAlpha = 255, .AlphaFormat = AC_SRC_ALPHA};
    UpdateLayeredWindow(layer.hwnd, hdcScreen, &pos, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hDib);
}

void DesktopIndicator::RenderLayer(MonitorLayer &layer, HDC hdcScreen, HDC hdcMem) {
    auto baseColors   = ParseMultiColorString(m_pCfg->textColor);
    auto colorStr     = BuildLayerColors(layer, baseColors.colors, baseColors.count);
    auto actualColors = ParseMultiColorString(colorStr);

    if (layer.symbolScales[0] < 0.01f) { layer.symbolScales.fill(1.0f); }
    int fontSize = MulDiv(m_pCfg->fontSize, layer.dpi, 96);

    // 1. 基准测量
    float unscaledW = 0;
    int   symCount  = static_cast<int>(m_text.size());
    for (int i = 0; i < symCount; ++i) {
        auto size = m_renderer->Measure(std::wstring(1, m_text[i]).c_str(), fontSize);
        unscaledW += static_cast<float>(size.cx);
    }
    int spacing = m_pCfg->charSpacing * m_renderer->Measure(L" ", fontSize).cx;
    if (symCount > 1) { unscaledW += static_cast<float>(spacing * (symCount - 1)); }
    float avgSymW = (symCount > 0) ? unscaledW / static_cast<float>(symCount) : 0.0f;
    int   centerW = std::max(static_cast<int>(unscaledW) + kPadding * 2, kMinWidth);

    // 2. 缩放计算
    bool isDragging = (m_draggingWindow && !m_editMode);
    ComputeScales(layer, avgSymW, isDragging);

    // 3. 缩放后布局
    auto metrics = CalcSymbolMetrics(*m_renderer, *m_pCfg, m_text, layer);

    // 4. 渲染提交
    PresentLayer(layer, metrics, centerW, actualColors, hdcScreen, hdcMem);
}

void DesktopIndicator::Render() {
    if (m_layers.empty() || m_pCfg == nullptr || m_renderer == nullptr) { return; }

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = (hdcScreen != nullptr) ? CreateCompatibleDC(hdcScreen) : nullptr;

    for (auto &l : m_layers) {
        RenderLayer(l, hdcScreen, hdcMem);
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
    int   savedPreset = (m_pCfg != nullptr) ? m_pCfg->positionPreset : -1;
    POINT savedWndPos = (m_pCfg != nullptr) ? m_pCfg->windowPos : POINT{0, 0};
    m_layers.clear();
    m_editMode = false;
    m_dragging = false;
    ReleaseCapture();

    EnumerateMonitors(GetModuleHandle(nullptr));

    if (wasVisible) {
        for (auto &l : m_layers) { ShowWindow(l.hwnd, SW_SHOW); }
        if (m_pCfg != nullptr && m_pCfg->showMode >= ShowMode::Show1s) {
            ShowTemporarily();
        }
    }
    Render();
    Log(L"[INFO] Rebuild done: " + std::to_wstring(m_layers.size()) + L" layers");
    if (savedPreset >= 0) {
        SetPositionPreset(savedPreset);
    } else if (m_pCfg->posInitialized && !m_layers.empty()) {
        m_pCfg->windowPos = savedWndPos;
        for (auto &l : m_layers) {
            POINT pos = {savedWndPos.x + (l.monitor.left - m_layers[0].monitor.left),
                         savedWndPos.y + (l.monitor.top - m_layers[0].monitor.top)};
            SetWindowPos(l.hwnd, nullptr, pos.x, pos.y, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    UpdateRenderTimer();
    StartBgSampleTimer();
    RegisterMouseWheelInput();
}

bool DesktopIndicator::HandleRawInput(HWND /*hwnd*/, LPARAM lp) {
    UINT sz = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, nullptr, &sz, sizeof(RAWINPUTHEADER)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    if (sz <= 0) { return false; }

    std::vector<BYTE> buf(sz);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                        buf.data(), &sz, sizeof(RAWINPUTHEADER))
        != sz) { return false; }

    auto *raw = reinterpret_cast<RAWINPUT *>(buf.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (raw->header.dwType != RIM_TYPEMOUSE
        || (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) == 0) { // NOLINT(cppcoreguidelines-pro-type-union-access)
        return false;
    }

    POINT cursorPt;
    GetCursorPos(&cursorPt);
    bool overIndicator = false;
    for (const auto &l : m_layers) {
        RECT wr;
        GetWindowRect(l.hwnd, &wr);
        if (PtInRect(&wr, cursorPt) != 0) {
            overIndicator = true;
            break;
        }
    }

    if (!overIndicator || !m_scrollSwitchFn || m_desktopCount <= 0 || m_editMode) { return false; }

    auto delta  = static_cast<int16_t>(raw->data.mouse.usButtonData); // NOLINT(cppcoreguidelines-pro-type-union-access)
    int  target = m_currentDesktop + ((delta > 0) ? -1 : 1);
    if (target >= 0 && target < m_desktopCount) {
        m_scrollSwitchFn(target);
        return true;
    }
    return false;
}

bool DesktopIndicator::HandleDragStart(HWND hwnd, LPARAM lp) {
    if (!m_editMode) { return false; }
    POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ClientToScreen(hwnd, &pt);
    auto it = std::ranges::find_if(m_layers,
                                   [hwnd](const MonitorLayer &l) {
                                       return l.hwnd == hwnd;
                                   });
    if (it == m_layers.end()) { return true; }

    int winLeft    = m_pCfg->windowPos.x + (it->monitor.left - m_layers[0].monitor.left);
    int winTop     = m_pCfg->windowPos.y + (it->monitor.top - m_layers[0].monitor.top);
    m_dragOffset.x = pt.x - winLeft;
    m_dragOffset.y = pt.y - winTop;
    m_dragging     = true;
    SetCapture(hwnd);
    return true;
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

    case WM_INDICATOR_MOVE_WINDOW:
        if (m_moveWindowFn && m_desktopCount > 0 && !m_editMode) {
            auto targetIdx = static_cast<int>(wp);
            auto hwndDrag  = reinterpret_cast<HWND>(lp); // NOLINT
            Log(L"[INFO] MoveWindow: desktop " + std::to_wstring(targetIdx));
            m_moveWindowFn(targetIdx, hwndDrag);
        }
        return 0;

    case WM_INPUT:
        if (HandleRawInput(hwnd, lp)) { return 0; }
        break;

    case WM_LBUTTONDOWN:
        if (HandleDragStart(hwnd, lp)) { return 0; }
        return DefWindowProcW(hwnd, msg, wp, lp);

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
        if (!m_editMode) { return DefWindowProcW(hwnd, msg, wp, lp); }
        if (m_dragging) {
            m_dragging = false;
            ReleaseCapture();
            if (m_pCfg != nullptr) { m_pCfg->positionPreset = -1; }
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        ToggleEditMode();
        return 0;

    case WM_RBUTTONDOWN:
        if (!m_editMode) { return DefWindowProcW(hwnd, msg, wp, lp); }
        SetEditMode(false);
        return 0;

    case WM_MOUSEWHEEL:
        if (m_editMode && m_pCfg != nullptr) {
            int delta   = GET_WHEEL_DELTA_WPARAM(wp);
            int oldSize = m_pCfg->fontSize;
            m_pCfg->fontSize += (delta > 0) ? 4 : -4;
            m_pCfg->fontSize = (std::clamp)(m_pCfg->fontSize, 12, 300);
            if (m_pCfg->fontSize != oldSize) {
                m_pCfg->SaveToIni();
                RebuildText();
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);

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
