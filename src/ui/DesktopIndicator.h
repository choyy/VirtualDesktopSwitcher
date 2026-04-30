// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef DESKTOP_INDICATOR_H
#define DESKTOP_INDICATOR_H

#include <windows.h>

#include <array>
#include <memory>
#include <string>

#include "util/Constants.h"

class FontRenderer;

class DesktopIndicator {
public:
    DesktopIndicator();
    ~DesktopIndicator();
    // 禁用复制和移动操作
    DesktopIndicator(const DesktopIndicator &)            = delete;
    DesktopIndicator &operator=(const DesktopIndicator &) = delete;
    DesktopIndicator(DesktopIndicator &&)                 = delete;
    DesktopIndicator &operator=(DesktopIndicator &&)      = delete;

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

    [[nodiscard]] bool                IsEditMode() const { return m_editMode; }
    [[nodiscard]] HWND                GetWindowHandle() const { return m_hwnd; }
    [[nodiscard]] const std::wstring &GetCurrentSymbol() const { return m_currentSymbol; }
    [[nodiscard]] const std::wstring &GetOtherSymbol() const { return m_otherSymbol; }
    [[nodiscard]] const std::wstring &GetEmptySymbol() const { return m_emptySymbol; }
    [[nodiscard]] const std::wstring &GetFontName() const { return m_fontName; }
    [[nodiscard]] int                 GetCharSpacing() const { return m_charSpacing; }
    [[nodiscard]] bool                IsAutoCheckUpdates() const { return m_autoCheckUpdates; }
    [[nodiscard]] int                 GetPositionPreset() const { return m_positionPreset; }

    void SetAutoCheckUpdates(bool v);

private:
    HWND                           m_hwnd = nullptr;
    std::unique_ptr<FontRenderer>  m_renderer;
    std::wstring                   m_text;
    std::wstring                   m_textColor = L"#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF";
    std::wstring                   m_previewColor;
    bool                           m_hasPreview     = false;
    std::wstring                   m_fontName       = L"Segoe UI Symbol";
    int                            m_fontSize       = 10;
    int                            m_charSpacing    = 0;
    int                            m_desktopCount   = 0;
    int                            m_currentDesktop = 0;
    std::wstring                   m_currentSymbol  = L"\u25C9";
    std::wstring                   m_otherSymbol    = L"\u25CB";
    std::wstring                   m_emptySymbol    = L"\u25CC";
    std::array<bool, kMaxDesktops> m_emptyDesktops{};
    bool                           m_posInitialized   = false;
    bool                           m_editMode         = false;
    int                            m_positionPreset   = -1;
    bool                           m_autoCheckUpdates = true;
    bool                           m_dragging         = false;
    POINT                          m_windowPos        = {.x = 0, .y = 0};
    POINT                          m_dragOffset       = {.x = 0, .y = 0};

    void RebuildText();
    void Render();
    void LoadConfig();
    void SaveConfig();
    void MoveByDelta(int dx, int dy);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT                 HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
};

#endif
