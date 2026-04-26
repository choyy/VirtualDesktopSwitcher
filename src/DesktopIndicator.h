// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef DESKTOP_INDICATOR_H
#define DESKTOP_INDICATOR_H

#include <windows.h>
#include <string>
#include <vector>

#define WM_OVERLAY_SETCOLOR (WM_USER + 10)
#define WM_OVERLAY_SETFONT  (WM_USER + 11)
#define WM_OVERLAY_EDITMODE (WM_USER + 12)
#define WM_OVERLAY_PREVIEW_COLOR (WM_USER + 13)
#define WM_OVERLAY_CANCEL_PREVIEW (WM_USER + 14)

class DesktopIndicator {
public:
    DesktopIndicator();
    ~DesktopIndicator();

    bool Initialize(HINSTANCE hInstance);
    void Show();
    void SetDesktopState(int count, int currentIndex, const std::vector<bool>& emptyDesktops);
    void SetColor(const std::wstring& hexColor);
    void SetColorPreview(const std::wstring& hexColor);
    void CancelPreview();
    void SetCurrentSymbol(const std::wstring& sym);
    void SetOtherSymbol(const std::wstring& sym);
    void SetEmptySymbol(const std::wstring& sym);
    void SetFontName(const std::wstring& name);
    void SetCharSpacing(int spacing);
    void ToggleEditMode();
    void SetEditMode(bool edit);
    bool IsEditMode() const { return m_editMode; }
    HWND GetWindowHandle() const { return m_hwnd; }
    const std::wstring& GetCurrentSymbol() const { return m_currentSymbol; }
    const std::wstring& GetOtherSymbol() const { return m_otherSymbol; }
    const std::wstring& GetEmptySymbol() const { return m_emptySymbol; }
    const std::wstring& GetFontName() const { return m_fontName; }
    int GetCharSpacing() const { return m_charSpacing; }
    bool IsAutoCheckUpdates() const { return m_autoCheckUpdates; }
    void SetAutoCheckUpdates(bool v);

private:
    HWND m_hwnd = nullptr;
    std::wstring m_text;
    std::wstring m_textColor = L"#648CFF";
    std::wstring m_previewColor;
    bool m_hasPreview = false;
    std::wstring m_fontName = L"Segoe UI Symbol";
    int m_fontSize = 60;
    int m_charSpacing = 0;
    int m_desktopCount = 0;
    int m_currentDesktop = 0;
    std::wstring m_currentSymbol = L"\u2609";
    std::wstring m_otherSymbol = L"\u25CB";
    std::wstring m_emptySymbol = L"\u25CB";
    std::vector<bool> m_emptyDesktops;
    bool m_editMode = false;
    bool m_autoCheckUpdates = true;
    bool m_dragging = false;
    POINT m_windowPos = {0, 0};
    POINT m_dragOffset = {0, 0};

    void RebuildText();
    void Render();
    void LoadConfig();
    void SaveConfig();
    void MoveByDelta(int dx, int dy);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
};

#endif
