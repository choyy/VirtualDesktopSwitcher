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
    if (m_isDraggingWindow) {
        m_indicator->HideDragOverlay();
        m_indicator->SetDraggingWindow(false);
    }
    m_clickPending     = false;
    m_isDraggingWindow = false;
    m_dragHwnd         = nullptr;
    m_lastHoverSymbol  = -1;
    m_enableDragSwitch = false;
}

bool WindowDragHandler::IsDragSwitch() const {
    if (m_dragMode == DragSwitchMode::Always) { return true; }
    if (m_dragMode == DragSwitchMode::Never) { return false; }
    int vk = (m_dragMode == DragSwitchMode::Alt) ? VK_MENU : VK_CONTROL;
    return (static_cast<UINT>(GetAsyncKeyState(vk)) & 0x8000u) != 0;
}

LRESULT CALLBACK WindowDragHandler::DragHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0 || s_instance == nullptr) {
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    auto *hs = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)

    if (wParam == WM_LBUTTONDOWN) {
        if (!s_instance->m_indicator->IsPtOnOverlay(hs->pt)) {
            s_instance->m_clickPending = true;
        }
    }

    if (wParam == WM_MOUSEMOVE && (s_instance->m_clickPending || s_instance->m_isDraggingWindow)) {
        GUITHREADINFO info       = {.cbSize = sizeof(info)};
        bool          inMoveSize = GetGUIThreadInfo(0, &info) != 0 && (info.flags & GUI_INMOVESIZE) != 0;

        if (!s_instance->m_isDraggingWindow) {
            if (inMoveSize) {
                CURSORINFO ci = {.cbSize = sizeof(ci)};
                if (GetCursorInfo(&ci) != 0 && ci.hCursor == s_hArrow) {
                    s_instance->m_clickPending     = false;
                    s_instance->m_dragHwnd         = info.hwndMoveSize;
                    s_instance->m_isDraggingWindow = true;
                    s_instance->m_enableDragSwitch = s_instance->IsDragSwitch();
                    if (s_instance->m_enableDragSwitch) {
                        s_instance->m_indicator->ShowDragOverlay();
                        s_instance->m_indicator->SetDraggingWindow(true);
                    }
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
                        if (s_instance->m_enableDragSwitch) {
                            Log(L"[INFO] MoveWindow: desktop " + std::to_wstring(symIdx));
                            PostMessageW(s_instance->m_hwndTarget, WM_WINDOW_DRAG_DROP,
                                         static_cast<WPARAM>(symIdx),
                                         reinterpret_cast<LPARAM>(s_instance->m_dragHwnd)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                        }
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
