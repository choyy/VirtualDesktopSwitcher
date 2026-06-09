#include "WindowDragHandler.h"

#include "ui/DesktopIndicator.h"
#include "util/Log.h"

namespace {

const HCURSOR s_hArrow = LoadCursor(nullptr, IDC_ARROW);

} // namespace

WindowDragHandler *WindowDragHandler::s_instance = nullptr;

WindowDragHandler::WindowDragHandler(DesktopIndicator *indicator)
    : m_indicator(indicator) {
    s_instance = this;
}

WindowDragHandler::~WindowDragHandler() {
    UninstallHook();
    if (s_instance == this) { s_instance = nullptr; }
}

bool WindowDragHandler::InstallHook(HWND hwndTarget) {
    if (m_hHook != nullptr) { return true; }
    m_hwndTarget = hwndTarget;
    m_hHook      = SetWindowsHookExW(WH_MOUSE_LL, DragHookProc,
                                     GetModuleHandleW(nullptr), 0);
    if (m_hHook != nullptr) {
        Log(L"[INFO] WindowDragHandler hook installed");
    } else {
        Log(L"[ERROR] WindowDragHandler hook install failed");
    }
    return m_hHook != nullptr;
}

void WindowDragHandler::UninstallHook() {
    if (m_hHook != nullptr) {
        UnhookWindowsHookEx(m_hHook);
        m_hHook = nullptr;
        Log(L"[INFO] WindowDragHandler hook uninstalled");
    }
}

void WindowDragHandler::ResetDragState() {
    if (m_dragging) {
        m_indicator->HideDragOverlay();
        m_indicator->SetDraggingWindow(false);
    }
    m_pendingDrag     = false;
    m_dragging        = false;
    m_dragHwnd        = nullptr;
    m_lastHoverSymbol = -1;
}

LRESULT CALLBACK WindowDragHandler::DragHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || s_instance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto *hs = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

    if (wParam == WM_LBUTTONDOWN) {
        if (!s_instance->m_indicator->IsPtOnOverlay(hs->pt)) {
            s_instance->m_pendingDrag = true;
        }
    }

    if (wParam == WM_MOUSEMOVE && (s_instance->m_pendingDrag || s_instance->m_dragging)) {
        GUITHREADINFO info       = {.cbSize = sizeof(info)};
        bool          inMoveSize = GetGUIThreadInfo(0, &info) != 0 && (info.flags & GUI_INMOVESIZE) != 0;

        if (!s_instance->m_dragging) {
            if (inMoveSize) {
                CURSORINFO ci = {.cbSize = sizeof(ci)};
                if (GetCursorInfo(&ci) != 0 && ci.hCursor == s_hArrow) {
                    s_instance->m_pendingDrag = false;
                    s_instance->m_dragHwnd    = info.hwndMoveSize;
                    s_instance->m_dragging    = true;
                    s_instance->m_indicator->ShowDragOverlay();
                    s_instance->m_indicator->SetDraggingWindow(true);
                }
            }
        } else {
            if (!inMoveSize) {
                s_instance->ResetDragState();
            } else {
                int symIdx = -1;
                if (s_instance->m_indicator->GetSymbolIndexAt(hs->pt, symIdx)) {
                    if (symIdx != s_instance->m_lastHoverSymbol) {
                        s_instance->m_lastHoverSymbol = symIdx;
                        Log(L"[INFO] MoveWindow: desktop " + std::to_wstring(symIdx));
                        PostMessageW(s_instance->m_hwndTarget, WM_WINDOW_DRAG_DROP,
                                     static_cast<WPARAM>(symIdx),
                                     reinterpret_cast<LPARAM>(s_instance->m_dragHwnd)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
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
