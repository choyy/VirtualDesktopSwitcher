// Contains code adapted from Catime (Apache 2.0)
// Original: https://github.com/vladelaina/Catime
#ifndef DESKTOP_OVERLAY_H
#define DESKTOP_OVERLAY_H

#include <windows.h>
#include <string>

#define WM_OVERLAY_SETCOLOR (WM_USER + 10)
#define WM_OVERLAY_SETFONT  (WM_USER + 11)
#define WM_OVERLAY_EDITMODE (WM_USER + 12)
#define WM_OVERLAY_PREVIEW_COLOR (WM_USER + 13)
#define WM_OVERLAY_CANCEL_PREVIEW (WM_USER + 14)
#define OVERLAY_COLOR_PREVIEW_MSG (WM_USER + 15)

class DesktopIndicator {
public:
    DesktopIndicator();
    ~DesktopIndicator();

    bool Initialize(HINSTANCE hInstance);
    void Show();
    void SetText(const std::wstring& text);
    void SetColor(const std::wstring& hexColor);
    void SetColorPreview(const std::wstring& hexColor);
    void CancelPreview();
    void ToggleEditMode();
    void SetEditMode(bool edit);
    bool IsEditMode() const { return m_editMode; }
    HWND GetWindowHandle() const { return m_hwnd; }

private:
    HWND m_hwnd = nullptr;
    std::wstring m_text = L"0*00";
    std::wstring m_textColor = L"#648CFF";
    std::wstring m_previewColor;
    bool m_hasPreview = false;
    std::wstring m_fontName = L"Segoe UI";
    int m_fontSize = 60;
    bool m_editMode = false;
    bool m_dragging = false;
    POINT m_windowPos = {0, 0};
    POINT m_dragOffset = {0, 0};

    void Render();
    void LoadConfig();
    void SaveConfig();
    void MoveByDelta(int dx, int dy);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
};

#endif
