// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#pragma once

#include <windows.h>

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "util/Utils.h"

class FontRenderer;

struct IndicatorConfig {
    std::wstring textColor      = L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF";
    std::wstring fontName       = L"Segoe UI Symbol";
    int          fontSize       = 20;
    int          charSpacing    = 0;
    std::wstring currentSymbol  = L"\u25C9";
    std::wstring otherSymbol    = L"\u25CB";
    std::wstring emptySymbol    = L"\u25CC";
    POINT        windowPos      = {};
    bool         posInitialized = false;
    int          positionPreset = -1;

    void LoadFromIni();
    void SaveToIni() const;
};

class DesktopIndicator {
public:
    DesktopIndicator();
    ~DesktopIndicator();
    DesktopIndicator(const DesktopIndicator &)            = delete;
    DesktopIndicator &operator=(const DesktopIndicator &) = delete;
    DesktopIndicator(DesktopIndicator &&)                 = delete;
    DesktopIndicator &operator=(DesktopIndicator &&)      = delete;

    void SetConfig(IndicatorConfig *cfg) { m_pCfg = cfg; }
    void SetOnConfigChanged(std::function<void()> cb) { m_onConfigChanged = std::move(cb); }

    bool Initialize(HINSTANCE hInstance);
    void Show();
    void SetDesktopState(int count, int currentIndex, const std::array<bool, kMaxDesktops> &emptyDesktops);
    void SetColor(const std::wstring &hexColor);
    void SetColorPreview(const std::wstring &hexColor);
    void CancelPreview();
    void SetCurrentSymbol(const std::wstring &sym);
    void SetOtherSymbol(const std::wstring &sym);
    void SetEmptySymbol(const std::wstring &sym);
    void SetFontName(const std::wstring &name);
    void SetCharSpacing(int spacing);
    void ToggleEditMode();
    void SetEditMode(bool edit);
    void SetPositionPreset(int preset);

    [[nodiscard]] bool IsEditMode() const { return m_editMode; }
    [[nodiscard]] HWND GetWindowHandle() const { return m_hwnd; }

private:
    HWND                           m_hwnd = nullptr;
    std::unique_ptr<FontRenderer>  m_renderer;
    IndicatorConfig               *m_pCfg = nullptr;
    std::function<void()>          m_onConfigChanged;
    std::wstring                   m_text;
    std::wstring                   m_previewColor;
    bool                           m_hasPreview     = false;
    int                            m_desktopCount   = 0;
    int                            m_currentDesktop = 0;
    std::array<bool, kMaxDesktops> m_emptyDesktops{};
    bool                           m_editMode   = false;
    bool                           m_dragging   = false;
    POINT                          m_dragOffset = {.x = 0, .y = 0};

    void RebuildText();
    void Render();
    void MoveByDelta(int dx, int dy);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT                 HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
};
